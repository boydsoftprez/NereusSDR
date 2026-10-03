// =================================================================
// tests/tst_daemon_media_controller.cpp  (NereusSDR)
// =================================================================
// R3 daemon media controller integration coverage over a real authenticated
// StationServer/StationClient control session. Synthetic I/Q is test-only;
// production reaches the source exclusively through RadioModel's tagged tap.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: Control logging lane: the media connection's selected pair
//               is logged when first known and on a change, with its rtt.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane, review round: the tail's start, the
//               pump's longest wait with its sequence step, and "RF start
//               not measured" without a placing send path. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the unkey line splits its padded
//               silence and the event lines place the over's underruns, ran
//               dry and catch-up bursts. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the unkey line's "line waits"
//               and the over's longest "tx" keepalive wait; a real
//               microphone line torn down (a new connection's start, and
//               the session's end) while its own thread delivers. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX watch follow-up: the torn-down line's test checks that
//               its packets came on the line's own thread and never on the
//               owner's, and that a new connection's line accepted and
//               rejected nothing of the old. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessController.h"
#include "core/FFTEngine.h"
#include "core/P2RadioConnection.h"
#include "core/DdcAssignment.h"
#include "core/WidebandFrameAccumulator.h"
#include "core/HpsdrModel.h"
#include "core/NoiseFloorEstimator.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/Ps3DisplayCodec.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteIqCodec.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "core/settings/SettingsProxy.h"
#include "gui/RemoteDisplayAllocator.h"
#include "fakes/LoopbackTransport.h"
#include "OperatorWording.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QPointer>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtEndian>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <mutex>
#include <numbers>
#include <numeric>
#include <thread>
#include <atomic>
#include <utility>
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
// R-R3-21: the waits on the real libdatachannel transport (ICE over
// loopback, then the first display frame) wait for the condition itself,
// with room for a heavily loaded machine: ten seconds fell short once
// under a full parallel build. Only the timeout is generous; nothing
// measured depends on it.
constexpr int kRealTransportWaitMs = 60'000;

// Messages the controller wrote to its own log category while installed.
QStringList g_daemonMediaMessages;
void captureDaemonMediaMessages(QtMsgType, const QMessageLogContext& context,
                                const QString& message)
{
    if (context.category && QByteArray(context.category) == "nereus.daemon.media") {
        g_daemonMediaMessages.append(message);
    }
}

bool isKeyframe(const QByteArray& frame)
{
    return frame.size() > 5 && (static_cast<quint8>(frame.at(5)) & 0x01) != 0;
}

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}

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
        if (sctpWindowBytes > 0) {
            const DisplaySendResult result = submitDisplay(bytes);
            return result == DisplaySendResult::Sent || result == DisplaySendResult::Queued;
        }
        if (!readyState) { return false; }
        displays.append(bytes);
        const auto callback = onDisplaySend;
        if (callback) { return callback(); }
        return true;
    }
    // With sctpWindowBytes set, models libdatachannel over usrsctp on a
    // link whose acknowledgements have not come back: SCTP takes a message
    // only while it fits beside the unacknowledged bytes; otherwise the
    // library holds one message, and a further one is Busy until the held
    // message goes out. `displays` records what went to SCTP, in order.
    DisplaySendResult submitDisplay(const QByteArray& bytes) override
    {
        if (sctpWindowBytes <= 0 && nextSubmitResult) {
            const DisplaySendResult result = *std::exchange(nextSubmitResult, std::nullopt);
            if (readyState && (result == DisplaySendResult::Sent
                               || result == DisplaySendResult::Queued)) {
                displays.append(bytes);
            }
            return result;
        }
        if (sctpWindowBytes <= 0) { return IMediaTransport::submitDisplay(bytes); }
        if (!readyState) { return DisplaySendResult::Refused; }
        if (!heldDisplay.isEmpty()) {
            ++busyDisplays;
            return DisplaySendResult::Busy;
        }
        if (unacknowledgedBytes + bytes.size() <= sctpWindowBytes) {
            unacknowledgedBytes += bytes.size();
            displays.append(bytes);
            return DisplaySendResult::Sent;
        }
        heldDisplay = bytes;
        ++queuedDisplays;
        return DisplaySendResult::Queued;
    }
    bool displayBusy() const override { return sctpWindowBytes > 0 && !heldDisplay.isEmpty(); }
    DisplaySendResult submitIq(const QByteArray& bytes) override
    {
        if (!readyState) { return DisplaySendResult::Refused; }
        if (stallIq || submitIqBusy) { return DisplaySendResult::Busy; }
        if (nextIqSubmitResult) {
            const DisplaySendResult result = *std::exchange(nextIqSubmitResult, std::nullopt);
            if (result == DisplaySendResult::Busy || result == DisplaySendResult::Refused) {
                return result;
            }
        }
        if (onIqSend) { onIqSend(bytes); }
        return DisplaySendResult::Sent;
    }
    bool iqBusy() const override { return stallIq; }
    // The peer acknowledges everything outstanding; a held message goes out.
    void acknowledgeDisplayWindow()
    {
        unacknowledgedBytes = 0;
        if (heldDisplay.isEmpty()) { return; }
        unacknowledgedBytes = heldDisplay.size();
        displays.append(std::exchange(heldDisplay, {}));
        emit displayWritable();
    }
    bool sendRtp(const QByteArray& packet) override
    {
        ++rtpAttempts;
        if (readyState) { rtpPackets.append(packet); }
        return readyState;
    }
    bool isReady() const override { return readyState; }
    bool losslessAudioNegotiated() const override { return losslessNegotiated; }
    std::optional<MediaIcePath> selectedPath() const override { return path; }
    std::optional<qint64> rttMs() const override { return rtt; }
    void becomeReady() { readyState = true; emit ready(); }

    // Control logging lane: the pair and rtt the transport reports.
    std::optional<MediaIcePath> path;
    std::optional<qint64> rtt;

    bool started{false};
    bool readyState{false};
    // Whether the (simulated) answer kept the L16 rtpmap; the real library's
    // answer does (tst_media_transport answerKeepsTheLosslessRtpMap).
    bool losslessNegotiated{false};
    StartOptions startOptions{Role::Answerer, 0};
    QList<QByteArray> displays;
    QList<QByteArray> rtpPackets;
    int rtpAttempts{0};
    std::function<bool()> onDisplaySend;
    qsizetype sctpWindowBytes{0};
    qsizetype unacknowledgedBytes{0};
    QByteArray heldDisplay;
    int busyDisplays{0};
    int queuedDisplays{0};
    // The outcome of the next submitDisplay() outside window mode.
    std::optional<DisplaySendResult> nextSubmitResult;
    bool stallIq{false};
    bool submitIqBusy{false}; // submit can race iqBusy() and remain Busy.
    std::optional<DisplaySendResult> nextIqSubmitResult;
    std::function<void(const QByteArray&)> onIqSend;
};

class ClosingControlTransport final : public Test::LoopbackTransport {
public:
    ClosingControlTransport() : LoopbackTransport(QStringLiteral("station")) {}
    void sendText(const QByteArray& wire) override
    {
        if (!closeOnOp.isEmpty()
            && wire.contains(QByteArray("\"op\":\"") + closeOnOp + '"')) {
            closeOnOp.clear();
            closeLink(QStringLiteral("test synchronous send closure"));
            return;
        }
        LoopbackTransport::sendText(wire);
    }
    QByteArray closeOnOp;
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

QJsonObject subscription(quint32 endpointId, quint32 revision, int sliceId,
                         double centreHz, int fftSize = 1024)
{
    return {{QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), fftSize},
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

QJsonObject tieredSubscription(quint32 endpointId, quint32 revision, int sliceId,
                               double centreHz, const QString& tier, int fftSize)
{
    QJsonObject request = subscription(endpointId, revision, sliceId, centreHz, fftSize);
    request.insert(QStringLiteral("tier"), tier);
    return request;
}

QJsonObject unsubscription(quint32 endpointId)
{
    return {{QStringLiteral("op"), QStringLiteral("unsubscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)}};
}

int messageCount(const QSignalSpy& messages, const QString& op, quint32 endpointId)
{
    int count = 0;
    for (const auto& call : messages) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId) {
            ++count;
        }
    }
    return count;
}

int messageIndex(const QSignalSpy& messages, const QString& op, quint32 endpointId)
{
    for (int index = 0; index < messages.count(); ++index) {
        const QJsonObject message = messages.at(index).at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId) {
            return index;
        }
    }
    return -1;
}

QJsonObject messageFor(const QSignalSpy& messages, const QString& op,
                       quint32 endpointId)
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

QJsonObject allocationFor(const QSignalSpy& messages, quint32 endpointId,
                          quint32 revision)
{
    for (auto it = messages.crbegin(); it != messages.crend(); ++it) {
        const QJsonObject message = it->at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("allocation-result")
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId
            && static_cast<quint32>(message.value(QStringLiteral("revision")).toInteger())
                == revision) {
            return message;
        }
    }
    return {};
}

int displayMessageCount(const QList<QByteArray>& messages, const QByteArray& magic)
{
    return static_cast<int>(std::count_if(messages.cbegin(), messages.cend(),
        [&magic](const QByteArray& bytes) { return bytes.startsWith(magic); }));
}

Ps3Snapshot maximumPs3Snapshot(quint64 generation, quint64 sequence)
{
    Ps3Snapshot snapshot;
    snapshot.channelId = 3;
    snapshot.sessionGeneration = generation;
    snapshot.sequence = sequence;
    snapshot.capturedAtUnixMilliseconds = static_cast<qint64>(sequence);
    snapshot.sampleCount = Ps3Snapshot::kMaxSampleCount;
    snapshot.correctionCount = Ps3Snapshot::kMaxCorrectionCount;
    const auto fill = [](std::vector<double>& values, int count, double base) {
        values.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            values.push_back(base + static_cast<double>(index) / 8192.0);
        }
    };
    fill(snapshot.x, snapshot.sampleCount, 0.0);
    fill(snapshot.ym, snapshot.sampleCount, 1.0);
    fill(snapshot.yc, snapshot.sampleCount, 2.0);
    fill(snapshot.ys, snapshot.sampleCount, 3.0);
    fill(snapshot.xmCorrection, snapshot.correctionCount, 4.0);
    fill(snapshot.ymCorrection, snapshot.correctionCount, 5.0);
    fill(snapshot.xaCorrection, snapshot.correctionCount, 6.0);
    fill(snapshot.yaCorrection, snapshot.correctionCount, 7.0);
    return snapshot;
}

QJsonObject audioControl(quint32 revision, bool enabled)
{
    return {{QStringLiteral("op"), QStringLiteral("audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled}};
}

// R-R3-23: the audio control of a GUI that understands audio profiles.
QJsonObject audioControl(quint32 revision, bool enabled, const QString& profile)
{
    QJsonObject control = audioControl(revision, enabled);
    control.insert(QStringLiteral("profile"), profile);
    return control;
}

// The media start of a GUI that understands audio profiles.
QJsonObject profileStart()
{
    return {{QStringLiteral("op"), QStringLiteral("start")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("audioProfileVersion"), 1}};
}

// Every audio context a raw control peer has received, in arrival order.
QList<QJsonObject> receivedAudioContexts(const Test::LoopbackTransport& peer)
{
    QList<QJsonObject> contexts;
    for (const QByteArray& wire : peer.received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::MediaControl
            && message.mediaPayload.value(QStringLiteral("op"))
                == QLatin1String("audio-context")) {
            contexts.append(message.mediaPayload);
        }
    }
    return contexts;
}

QStringList sortedKeys(const QJsonObject& object)
{
    QStringList keys = object.keys();
    keys.sort();
    return keys;
}

// The minor-7 audio context, key for key and JSON type for type: what every
// GUI built before the audio status detail parses.
bool hasLegacyAudioContextShape(const QJsonObject& context)
{
    const QStringList legacyKeys{
        QStringLiteral("connectionId"), QStringLiteral("enabled"),
        QStringLiteral("firstSequence"), QStringLiteral("firstTimestamp"),
        QStringLiteral("generation"), QStringLiteral("op"),
        QStringLiteral("revision"), QStringLiteral("ssrc")};
    return sortedKeys(context) == legacyKeys
        && context.value(QStringLiteral("op")) == QLatin1String("audio-context")
        && context.value(QStringLiteral("connectionId")).isString()
        && context.value(QStringLiteral("enabled")).isBool()
        && context.value(QStringLiteral("revision")).isDouble()
        && context.value(QStringLiteral("generation")).isDouble()
        && context.value(QStringLiteral("ssrc")).isDouble()
        && context.value(QStringLiteral("firstSequence")).isDouble()
        && context.value(QStringLiteral("firstTimestamp")).isDouble();
}

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    P2RadioConnection p2;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<FakeTransport> mediaTransport;
    // When set, Core's media transport comes from here instead of a fake.
    std::function<IMediaTransport*(QObject*)> realTransport;
    QPointer<ClosingControlTransport> stationTransport;
    qint64 nowNs{0};
    // A run in real time paces displays on the wall clock instead of nowNs.
    bool realClock{false};
    QElapsedTimer realTimer;
    // TX watch follow-up: while set, the names of the threads that read the
    // clock (the microphone line's receiver reads it on the delivering
    // thread).
    std::atomic<bool> recordClockThreads{false};
    std::mutex clockThreadsLock;
    QSet<QString> clockThreads;
    QPointer<QTimer> manualSender;
    DaemonMediaController controller;
    int sliceId{-1};
    int spareSliceId{-1};
    int streamIndex{-1};

    explicit Harness(std::optional<DisplayBudgetLimits> limits = std::nullopt)
        : settings(directory.filePath(QStringLiteral("station.settings")))
        // R-R3-49: one TLS certificate per test process (UpgradedCoreToken.h).
        , server(&radio, settings,
                 NereusSDR::Test::withSharedTlsIdentity(
                     NereusSDR::Test::seedUpgradedCoreToken(directory.path())))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         if (realTransport) { return realTransport(parent); }
                         mediaTransport = new FakeTransport(parent);
                         return mediaTransport;
                     },
                     [this] {
                         if (recordClockThreads.load(std::memory_order_relaxed)) {
                             const QString name = QThread::currentThread()->objectName();
                             const std::lock_guard<std::mutex> lock(clockThreadsLock);
                             clockThreads.insert(name);
                         }
                         return realClock ? realTimer.nsecsElapsed() : nowNs;
                     })
    {
        Q_ASSERT(directory.isValid());
        realTimer.start();
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        sliceId = radio.addSlice();
        Q_ASSERT(sliceId >= 0);
        // RadioModel deliberately keeps one local slice alive. Keep this
        // second slice so removal below exercises its real removal signal.
        // Do not put addSlice() inside Q_ASSERT: release test builds compile
        // assertions out and would silently skip this required setup.
        spareSliceId = radio.addSlice();
        Q_ASSERT(spareSliceId >= 0);
        SliceModel* const slice = radio.sliceById(sliceId);
        Q_ASSERT(slice != nullptr);
        streamIndex = slice->streamIndex();
        Q_ASSERT(streamIndex >= 0 && radio.streamActive(streamIndex));
        server.setMediaEnabled(true);
        if (limits) {
            // R-R3-01/R-R3-08: the pacer never refuses an update in a
            // budget session, including when the controller is torn down.
            QTest::failOnWarning(QRegularExpression(
                QStringLiteral("refused invalid display pacer state update")));
            QVERIFY(server.setDisplayBudgetLimits(*limits));
        }
    }

    void enableWidebandSource()
    {
        p2.setBoardForTest(HPSDRHW::Saturn);
        radio.injectConnectionForTest(&p2);
        RadioInfo info;
        info.protocol = ProtocolVersion::Protocol2;
        radio.setLastRadioInfoForTest(info);
        radio.wireWidebandConnectionForTest();
    }

    void establishSession()
    {
        auto* stationLink = new ClosingControlTransport;
        stationTransport = stationLink;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        // Both ends, not just Core's. Core counts the session ready once it
        // has sent the snapshot-complete marker; the GUI end is ready only
        // once that queued marker has reached it, which a loaded machine can
        // leave for a later event-loop pass. Until then
        // StationClient::sendMediaControl() refuses every message.
        QTRY_VERIFY(server.mediaAvailable() && client.mediaAvailable());
    }

    void startReadyPeer(bool iq = false)
    {
        // The exact condition sendMediaControl() checks before it sends.
        QTRY_VERIFY(client.mediaAvailable());
        QJsonObject start{
            {QStringLiteral("op"), QStringLiteral("start")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}};
        if (iq) { start.insert(QStringLiteral("remoteIqVersion"), 1); }
        QVERIFY(client.sendMediaControl(start, client.sessionEpoch()));
        QTRY_VERIFY(mediaTransport);
        QVERIFY(mediaTransport->started);
        QVERIFY(mediaTransport->startOptions.localAudioSsrc != 0);
        mediaTransport->becomeReady();
    }

    void feedRadio(double cyclesPerSample = 0.125)
    {
        // This invokes the actual production RadioModel tap. The daemon
        // source's DirectConnection bridge owns the worker-thread hop.
        const bool invoked = QMetaObject::invokeMethod(
            &radio, "rawIqDataForStream", Qt::DirectConnection,
            Q_ARG(int, streamIndex),
            Q_ARG(QVector<float>, syntheticIq(1026, cyclesPerSample)));
        QVERIFY(invoked);
    }

    void feedZeroRadio(int complexSamples = 1026)
    {
        // Exercise the same tagged RadioModel tap with a known full-source
        // floor: FFTEngine maps every zero-power bin to -200 dBFS.
        const QVector<float> zeros(complexSamples * 2, 0.0f);
        const bool invoked = QMetaObject::invokeMethod(
            &radio, "rawIqDataForStream", Qt::DirectConnection,
            Q_ARG(int, streamIndex), Q_ARG(QVector<float>, zeros));
        QVERIFY(invoked);
    }

    void useManualDisplayTicks()
    {
        for (QTimer* timer : controller.findChildren<QTimer*>(
                 QString(), Qt::FindDirectChildrenOnly)) {
            if (timer->interval() == static_cast<int>(kDisplaySenderIntervalMs)) {
                manualSender = timer;
                // Production keeps starting this same timer. A long fixture
                // interval leaves the real timeout slot under explicit control.
                timer->setInterval(60'000);
                return;
            }
        }
        QFAIL("display sender timer missing");
    }

    void sendDisplayTick()
    {
        QTimer* sender = manualSender;
        const auto timers = controller.findChildren<QTimer*>(
            QString(), Qt::FindDirectChildrenOnly);
        for (QTimer* timer : timers) {
            if (timer->interval() == static_cast<int>(kDisplaySenderIntervalMs)) {
                sender = timer;
                break;
            }
        }
        QVERIFY(sender);
        sender->stop();
        QVERIFY(QMetaObject::invokeMethod(sender, "timeout", Qt::DirectConnection));
    }

    void finish()
    {
        client.disconnectFromStation(QStringLiteral("test complete"));
    }
};

} // namespace

class TstDaemonMediaController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
    }

    void authenticatedControlProducesContextThenDecodedDisplayAndUnsubscribes();
    // R-IOS-13, R-R3-42 (2026-09-27).
    void unkeyLineCarriesTheMicrophonePathsLatency();
    void unkeyEventLinesPlaceTheOversDropouts();
    void extendedPermissionFirstCaptureAndSharedEndpointLifetimes();
    void widebandSourceReplacementAndSessionRetirement();
    void olderPeerKeepsLegacyContextAndCannotAcquireWideband();
    void minorSevenPeerReceivesLegacyAudioContexts();
    void minorEightAudioContextsCarryEncoderOrReason();
    void configuredAudioBitrateReachesOfferAndContext();
    void aDeviceChoosesItsOwnMeasuredOpusBitrate();
    void aDeviceThatNeverAsksGetsTheCoresBitrate();
    void losslessRequestSwitchesAtABlockBoundaryAndAcknowledges();
    void losslessRefusalKeepsOpusAndSaysWhy_data();
    void losslessRefusalKeepsOpusAndSaysWhy();
    void minorSevenPeerCannotAskForAnAudioProfile();
    void startNeedsAnAudioProfileVersionOfAtLeastOne();
    void displayFramesAndAudioShareTheProducerClock();
    void clockProbeIsAnsweredWithTheCoreClockAndCapture();
    void minorEightPeerReceivesTodaysSpectrumContext();
    void minorNinePeerReceivesTheGrant();
    void currentMinorSpectrumContextsReportTheGrant();
    void localCaptureDuringConnectingGetsIdentityBeforeFirstAdcRow();
    void synchronousDisplayClosureRetiresDemandAndAllowsNewPeer_data();
    void synchronousDisplayClosureRetiresDemandAndAllowsNewPeer();
    void synchronousControlClosureRetiresDemand_data();
    void synchronousControlClosureRetiresDemand();
    void aPeerTheCoreDropsIsToldToTheAppAtOnce_data();
    void aPeerTheCoreDropsIsToldToTheAppAtOnce();
    void aNewStartReplacesTheSessionsHalfOpenPeer();
    void staleRevisionAndWrongEpochPreserveTheActiveSource();
    void streamRemovalRetiresEndpointAndSource();
    void nonoverlappingCropIsRejectedBeforeSourceActivation();
    void fullSourceNoiseFloorFollowsContextCalibrationCadenceAndRetirement();
    void nonzeroNoiseFloorMatchesLocalFftBeforeCropAndQuantization();
    void configuredBudgetReturnsExactAllocationResultsAndRejectsOvercommit();
    void revisionedUnsubscribeCannotRetireOrResurrectNewerState();
    void boundedNonliveCacheNeverResurrectsEvictedEndpointIds();
    void rapidReplacementCannotMintSpectrumBurstCredit();
    void ps3PinsCurrentMultipartFrameAndPromotesOnlyLatest();
    void runtimeCapDecreaseAllowsOnlyComponentwiseReductions();
    void coreBusyLowersThenRestoresTheBudget();
    void eightWidePansAtTheCeilingGetTheirPlannedFrameRates();
    void pureSignalKeepsItsDisplayShareWhileSpectrumIsContended();
    void legacyPansOnTwoSourcesKeepTheirEvenShare();
    void failedDisplayAttemptDebitsCreditAndRecoversWithKeyframe();
    void exhaustedDisplayCreditDoesNotBlockAudioRtp();
    void mediaPeerReplacementDoesNotMintDisplayCredit();
    void mixedSpectrumAndPs3StayWithinInjectedIntervalBoundsAndBothProgress();
    void failedMiddlePs3ChunkDropsRemainderPromotesLatestAndDoesNotRefund();
    void ps3SnapshotCompletesWhileAcknowledgementsLag();
    void spectrumAndPs3ShareALaggingWindowAndBothProgress();
    void radioProductionRestartWithinEpochDoesNotMintDisplayCredit();
    void replayedAllocationResultCanSynchronouslyRetireControllerState();
    void sourceRetirementPublishesZeroChargeAtLatestOperationRevision();
    void failedSourceUpdateReleasesAllocationAndCanRecover();
    void sharedEngineKeepsOtherPanWhileNeighbourChurns();
    void sharedEngineKeepsOtherPanAcrossFrameRates();
    void grantReportsLargestSizeSharedEngineAndSourceBins();
    void budgetChargesGrantedPixels();
    void regrantAfterNeighbourLeavesStaysWithinAdmittedCharge();
    void sharedEngineRegrantsEverySurvivorWhenItsSizerLeaves();
    void subscribeDecimationReachesTheEndpointsEngine();
    void peerBelowTheGrantMinorCannotAskForDecimation();
    void destroyingControllerWithLiveBudgetSessionIsQuiet();
    void outOfRangeRequestsAreRejectedAndLeaveEndpointUntouched();
    void displayDiagnosticsMeasureSentFramesRefusalsAndErrors();
    void theMediaPairIsLoggedWhenKnownAndOnChange();
    void aTxKeepaliveChecksTheMediaPairAtMostOnceASecond();
    void realDisplayErrorIsCountedAndLoggedOnce();
    // TX mic thread fix round 2.
    void realMicLineTornDownWhileItsThreadDelivers_data();
    void realMicLineTornDownWhileItsThreadDelivers();
    void displayDiagnosticsLineReportsBytesAndFragments();
    void staleEpochAndForeignConnectionLeaveEndpointsUntouched();
    void olderAppNeverReceivesReceiverStreams();
    void receiverStreamRunsBesideTheMainWithoutRestartingIt();
    void staleReceiverRevisionIsIgnoredAndAFifthStreamIsRefused();
    void receiverStreamFollowsTheSessionAudioProfile();
    void receiverStreamRetiresOnSliceRadioAndSessionEnd();
    void olderAppGetsTheWholeProgramAndNoHeadphonesMix();
    void headphonesMixRunsWhileAReceiverIsOnTheHeadphones();
    void headphonesMixFollowsTheProfileAndTheRadio();
    void radioDropKeepsTheHeadphonesReasonWhenNothingIsRouted();
    void radioDropTellsAnAppWaitingOnMediaThatTheRadioIsGone();
    void aBoundControllerServesItsOwnSessionAndHearsItsOwnMix();
    void aListenedSliceJoinsTheMixAtTheListenersOwnLevel();
    void rawIqRunsForSixtySecondsInSequenceAndRetires();
    void stalledRawIqFailsWithoutBlockingControl();
    void rawIqBusyRetryDebitsOnlyOnce();
    void rawIqSubmitBusyStopsAtDeadline();
    void synchronousIqClosureRetiresPeerBeforeNextSend();
    void rawIqControlRejectsMalformedAndUnownedRequests();
};

// iPhone app Task 76 (ruling 9.1): a controller bound to a media session
// serves that session alone and takes its own owner mix of the Core's
// audio; one bound to another session never starts; both let their owner
// mixes go when their session ends.
void TstDaemonMediaController::aBoundControllerServesItsOwnSessionAndHearsItsOwnMix()
{
    Harness h;
    h.establishSession();
    const quint64 epoch = h.server.mediaSessionEpoch();
    QVERIFY(epoch != 0);
    QCOMPARE(h.controller.sessionEpoch(), epoch);
    AudioEngine* const engine = h.radio.audioEngine();
    QVERIFY(h.controller.ownerMixSlot() >= 0);
    // The session's device owns both slices, so its mix carries both.
    QTRY_COMPARE(engine->ownerMixSliceMask(h.controller.ownerMixSlot()),
                 (1u << h.sliceId) | (1u << h.spareSliceId));
    {
        DaemonMediaController other(&h.server, &h.radio, epoch + 1,
                                    std::make_shared<DaemonSharedSpectrum>(&h.radio));
        QCOMPARE(other.sessionEpoch(), quint64{0});
        QCOMPARE(other.ownerMixSlot(), -1);
        DaemonMediaController bound(&h.server, &h.radio, epoch,
                                    std::make_shared<DaemonSharedSpectrum>(&h.radio));
        QCOMPARE(bound.sessionEpoch(), epoch);
        QVERIFY(bound.ownerMixSlot() >= 0);
        QVERIFY(bound.ownerMixSlot() != h.controller.ownerMixSlot());
        QCOMPARE(engine->ownerMixCount(), 2);
    }
    QCOMPARE(engine->ownerMixCount(), 1);
    h.finish();
    QTRY_COMPARE(h.controller.sessionEpoch(), quint64{0});
    QCOMPARE(h.controller.ownerMixSlot(), -1);
    QCOMPARE(engine->ownerMixCount(), 0);
}

// Slice control plan Task 6 (rulings Q3, Q4): a slice the session's device
// only listens to joins its owner mix as a listen lane, at the slice's AF
// level to start and then at the level the device sets; the controller's
// AF and mute do not move it, and it leaves when the device stops
// listening. Nothing is acquired.
void TstDaemonMediaController::aListenedSliceJoinsTheMixAtTheListenersOwnLevel()
{
    Harness h;
    h.establishSession();
    const quint64 epoch = h.server.mediaSessionEpoch();
    QVERIFY(epoch != 0);
    AudioEngine* const engine = h.radio.audioEngine();
    const int slot = h.controller.ownerMixSlot();
    QVERIFY(slot >= 0);
    const QByteArray device = h.server.mediaSessionDevice(epoch);
    QVERIFY(!device.isEmpty());
    SliceAccessController* const access = h.server.sliceAccessController();
    QVERIFY(access != nullptr);
    SliceOwnership* const ownership = h.radio.sliceOwnership();
    SliceModel* const spare = h.radio.sliceById(h.spareSliceId);
    QVERIFY(spare != nullptr);
    spare->setAfGain(40);

    // The Core's own desktop controls the spare slice; the device listens.
    ownership->setOwner(h.spareSliceId, SliceOwnership::stationDevice());
    QVERIFY(ownership->join(device, h.spareSliceId));
    const quint32 spareBit = 1u << h.spareSliceId;
    QTRY_COMPARE(engine->ownerMixSliceMask(slot) & spareBit, 0u);
    QTRY_COMPARE(engine->ownerMixListenMask(slot), spareBit);
    QCOMPARE(engine->ownerMixListenLevel(slot, h.spareSliceId), 0.4f);

    // Its own level and mute.
    SliceOwnership::SliceRef ref;
    ref.sliceId = h.spareSliceId;
    ref.incarnation = ownership->incarnation(h.spareSliceId);
    QVERIFY(access->setListenLevel(device, ref, 0.25, false).accepted);
    QCOMPARE(engine->ownerMixListenLevel(slot, h.spareSliceId), 0.25f);
    QVERIFY(access->setListenLevel(device, ref, 0.25, true).accepted);
    QCOMPARE(engine->ownerMixListenLevel(slot, h.spareSliceId), 0.0f);
    QVERIFY(access->setListenLevel(device, ref, 0.25, false).accepted);

    // The controller's AF and mute leave it alone.
    spare->setAfGain(0);
    spare->setMuted(true);
    QCOMPARE(engine->ownerMixListenLevel(slot, h.spareSliceId), 0.25f);
    QCOMPARE(engine->ownerMixListenMask(slot), spareBit);
    QCOMPARE(engine->ownerMixCount(), 1);

    // It leaves when the device stops listening.
    QVERIFY(ownership->leave(device, h.spareSliceId));
    QTRY_COMPARE(engine->ownerMixListenMask(slot), 0u);
    QCOMPARE(engine->ownerMixSliceMask(slot) & spareBit, 0u);
    h.finish();
}

void TstDaemonMediaController::configuredBudgetReturnsExactAllocationResultsAndRejectsOvercommit()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 7});
    harness.establishSession();
    QVERIFY(harness.server.displayBudgetAvailable());
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();

    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    const QJsonObject first = subscription(70, 1, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(first, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 70, 1).isEmpty());
    const QJsonObject accepted = allocationFor(controls, 70, 1);
    QCOMPARE(accepted.size(), 11);
    QCOMPARE(accepted.value(QStringLiteral("accepted")).toBool(), true);
    QCOMPARE(accepted.value(QStringLiteral("budgetGeneration")).toInteger(), qint64{7});
    QCOMPARE(accepted.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{1});
    QCOMPARE(accepted.value(QStringLiteral("applicationBytesPerSecond")).toInteger(),
             static_cast<qint64>(cost.charge.applicationBytesPerSecond));
    QCOMPARE(accepted.value(QStringLiteral("spectrumSampleUnitsPerSecond")).toInteger(),
             static_cast<qint64>(cost.charge.spectrumSampleUnitsPerSecond));
    QCOMPARE(accepted.value(QStringLiteral("messagesPerSecond")).toInteger(), qint64{60});

    QVERIFY(harness.client.sendMediaControl(
        subscription(71, 1, harness.sliceId, centre), harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 71, 1).isEmpty());
    const QJsonObject refused = allocationFor(controls, 71, 1);
    QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(), false);
    QCOMPARE(refused.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{0});
    QCOMPARE(refused.value(QStringLiteral("applicationBytesPerSecond")).toInteger(), qint64{0});
    QCOMPARE(harness.controller.activeEndpointCount(), 1);

    QJsonObject malformed = subscription(72, 1, harness.sliceId, centre);
    malformed.insert(QStringLiteral("unexpected"), true);
    QVERIFY(harness.client.sendMediaControl(malformed, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 72, 1).isEmpty());
    QVERIFY(!allocationFor(controls, 72, 1).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 1);
    harness.finish();
}

void TstDaemonMediaController::revisionedUnsubscribeCannotRetireOrResurrectNewerState()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 9});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QVERIFY(harness.client.sendMediaControl(
        subscription(80, 2, harness.sliceId, centre), harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 80, 2).value(QStringLiteral("accepted")).toBool());

    const auto unsubscribe = [&](quint32 revision) {
        return harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("unsubscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), 80},
            {QStringLiteral("revision"), static_cast<qint64>(revision)}},
            harness.client.sessionEpoch());
    };
    QVERIFY(unsubscribe(1));
    QTRY_VERIFY(!allocationFor(controls, 80, 1).isEmpty());
    QCOMPARE(allocationFor(controls, 80, 1).value(QStringLiteral("accepted")).toBool(), false);
    QCOMPARE(harness.controller.activeEndpointCount(), 1);

    QVERIFY(unsubscribe(3));
    QTRY_VERIFY(allocationFor(controls, 80, 3).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 0);

    QJsonObject malformedReuse = subscription(80, 4, harness.sliceId, centre);
    malformedReuse.insert(QStringLiteral("unexpected"), true);
    QVERIFY(harness.client.sendMediaControl(malformedReuse, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 80, 4).isEmpty());
    QCOMPARE(allocationFor(controls, 80, 4).value(QStringLiteral("accepted")).toBool(), false);
    QVERIFY(harness.client.sendMediaControl(
        subscription(80, 5, harness.sliceId, centre), harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 80, 5).isEmpty());
    QCOMPARE(allocationFor(controls, 80, 5).value(QStringLiteral("accepted")).toBool(), false);
    QCOMPARE(harness.controller.activeEndpointCount(), 0);
    harness.finish();
}

void TstDaemonMediaController::boundedNonliveCacheNeverResurrectsEvictedEndpointIds()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 11});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);

    for (quint32 endpointId = 1; endpointId <= 65; ++endpointId) {
        QJsonObject invalid = subscription(endpointId, 1, std::numeric_limits<int>::max(), centre);
        QVERIFY(harness.client.sendMediaControl(invalid, harness.client.sessionEpoch()));
        QTRY_VERIFY(!allocationFor(controls, endpointId, 1).isEmpty());
        QVERIFY(!allocationFor(controls, endpointId, 1)
                     .value(QStringLiteral("accepted")).toBool());
    }
    QCOMPARE(harness.controller.activeEndpointCount(), 0);

    // A failed initial request still in the bounded cache may retry with a
    // newer revision, while an evicted identifier at or below high-water may
    // never be resurrected.
    QJsonObject retry = subscription(65, 2, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(retry, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 65, 2).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 1);

    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("unsubscribe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 65},
        {QStringLiteral("revision"), 3}}, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 65, 3).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 0);

    retry = subscription(1, 2, harness.sliceId, centre);
    retry.insert(QStringLiteral("unexpected"), true);
    QVERIFY(harness.client.sendMediaControl(retry, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 1, 2).isEmpty());
    QVERIFY(!allocationFor(controls, 1, 2).value(QStringLiteral("accepted")).toBool());
    retry = subscription(1, 3, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(retry, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 1, 3).isEmpty());
    QVERIFY(!allocationFor(controls, 1, 3).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 0);
    harness.finish();
}

void TstDaemonMediaController::rapidReplacementCannotMintSpectrumBurstCredit()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(4096, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 13});
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(90, 1, harness.sliceId, centre);
    request.insert(QStringLiteral("pixels"), 4096);
    // Include enough source bins that the first actual frame exhausts the
    // remaining credit needed for another worst-case preflight. The budget
    // charges granted pixels, so the FFT must supply all 4096 of them.
    request.insert(QStringLiteral("spanHz"), 192000.0);
    request.insert(QStringLiteral("fftSize"), 4096);
    request.insert(QStringLiteral("fps"), 60);

    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 90, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 90)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());
    harness.sendDisplayTick();
    QTRY_COMPARE(harness.mediaTransport->displays.size(), 1);

    for (quint32 revision = 2; revision <= 8; ++revision) {
        request.insert(QStringLiteral("revision"), static_cast<qint64>(revision));
        QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
        QTRY_VERIFY(allocationFor(controls, 90, revision)
                        .value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(([&] {
            harness.feedRadio(0.125 + static_cast<double>(revision) / 1024.0);
            return messageFor(controls, QStringLiteral("context"), 90)
                .value(QStringLiteral("revision")).toInteger() == revision;
        })());
        harness.sendDisplayTick();
    }
    quint64 attemptedAtZero = 0;
    for (const QByteArray& bytes : std::as_const(harness.mediaTransport->displays)) {
        attemptedAtZero += static_cast<quint64>(bytes.size());
    }
    QVERIFY(attemptedAtZero <= kMaximumSpectrumDisplayFrameBytes);
    QCOMPARE(harness.mediaTransport->displays.size(), 1);

    harness.nowNs = 1'000'000'000;
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.1875);
        harness.sendDisplayTick();
        return harness.mediaTransport->displays.size() == 2;
    })());
    harness.finish();
}

void TstDaemonMediaController::ps3PinsCurrentMultipartFrameAndPromotesOnlyLatest()
{
    const DisplayBudgetCharge charge = ps3DisplayCharge();
    Harness harness(DisplayBudgetLimits{charge.applicationBytesPerSecond, 1, 15});
    harness.useManualDisplayTicks();
    harness.establishSession();
    harness.startReadyPeer();
    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    const quint64 generation = facade->displayGeneration();
    const Ps3Snapshot first = maximumPs3Snapshot(generation, 1);
    const Ps3Snapshot latest = maximumPs3Snapshot(generation, 2);
    QCOMPARE(Ps3DisplayCodec::encode(first).size(), 3);

    facade->displaySnapshotReady(first);
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1);
    facade->displaySnapshotReady(latest);
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1); // no same-time credit

    for (int attempt = 1; attempt <= 5; ++attempt) {
        harness.nowNs += 50'000'000;
        harness.sendDisplayTick();
    }
    QCOMPARE(harness.mediaTransport->displays.size(), 6);

    Ps3DisplayAssembler assembler(generation);
    QList<quint64> completed;
    for (const QByteArray& bytes : std::as_const(harness.mediaTransport->displays)) {
        QString error;
        const auto frame = assembler.accept(bytes, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        if (frame) { completed.append(frame->sequence); }
    }
    QCOMPARE(completed, QList<quint64>({1, 2}));
    harness.finish();
}

void TstDaemonMediaController::runtimeCapDecreaseAllowsOnlyComponentwiseReductions()
{
    const SpectrumDisplayCost initialCost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{initialCost.charge.applicationBytesPerSecond,
                                        initialCost.charge.spectrumSampleUnitsPerSecond, 17});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(95, 1, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 95, 1).value(QStringLiteral("accepted")).toBool());

    const SpectrumDisplayCost loweredCap = *spectrumDisplayCost(64, 30, false);
    QVERIFY(harness.server.setDisplayBudgetLimits(
        {loweredCap.charge.applicationBytesPerSecond,
         loweredCap.charge.spectrumSampleUnitsPerSecond, 18}));
    QTRY_COMPARE(harness.client.remoteDisplayBudgetLimits()->generation, quint32{18});

    request.insert(QStringLiteral("revision"), 2);
    request.insert(QStringLiteral("fps"), 59);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 95, 2).value(QStringLiteral("accepted")).toBool());

    request.insert(QStringLiteral("revision"), 3);
    request.insert(QStringLiteral("fps"), 60);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 95, 3).isEmpty());
    const QJsonObject refused = allocationFor(controls, 95, 3);
    QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(refused.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{2});
    const SpectrumDisplayCost retained = *spectrumDisplayCost(128, 59, false);
    QCOMPARE(refused.value(QStringLiteral("applicationBytesPerSecond")).toInteger(),
             static_cast<qint64>(retained.charge.applicationBytesPerSecond));
    request.insert(QStringLiteral("revision"), 4);
    request.insert(QStringLiteral("fps"), 59);
    request.insert(QStringLiteral("sliceId"), std::numeric_limits<int>::max());
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 95, 4).isEmpty());
    const QJsonObject sourceRefused = allocationFor(controls, 95, 4);
    QVERIFY(!sourceRefused.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(sourceRefused.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{2});
    QCOMPARE(sourceRefused.value(QStringLiteral("applicationBytesPerSecond")).toInteger(),
             static_cast<qint64>(retained.charge.applicationBytesPerSecond));

    QJsonObject older = request;
    older.insert(QStringLiteral("revision"), 3);
    older.insert(QStringLiteral("sliceId"), harness.sliceId);
    older.insert(QStringLiteral("fps"), 58);
    QVERIFY(harness.client.sendMediaControl(older, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 95, 3).isEmpty());
    QCOMPARE(allocationFor(controls, 95, 3)
                 .value(QStringLiteral("acceptedRevision")).toInteger(), qint64{2});

    QVERIFY(harness.server.setDisplayBudgetLimits(
        {initialCost.charge.applicationBytesPerSecond,
         initialCost.charge.spectrumSampleUnitsPerSecond, 19}));
    const int priorRevisionFourResults = messageCount(
        controls, QStringLiteral("allocation-result"), 95);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(messageCount(controls, QStringLiteral("allocation-result"), 95)
                > priorRevisionFourResults);
    const QJsonObject replayed = allocationFor(controls, 95, 4);
    QVERIFY(!replayed.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(replayed.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{2});
    QCOMPARE(harness.controller.activeEndpointCount(), 1);
    harness.finish();
}

// R-R3-08/37/40: the governor's step reaches the live session as a new
// limits generation with its reason: the pacer takes it without complaint
// (Harness fails on a refused pacer update), admission lets the endpoint
// reduce but not grow, and the restore brings back the requested quality
// and clears the reason.
void TstDaemonMediaController::coreBusyLowersThenRestoresTheBudget()
{
    const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
    Harness harness(ceiling);
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QCOMPARE(harness.client.remoteDisplayBudgetLimits()->generation, quint32{1});
    QCOMPARE(harness.client.remoteDisplayBudgetReason(), DisplayBudgetReason::None);

    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(97, 1, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 97, 1).value(QStringLiteral("accepted")).toBool());
    const DisplayBudgetCharge requested = spectrumDisplayCost(128, 60, false)->charge;
    QCOMPARE(harness.controller.acceptedDisplayCharge(), requested);
    // Normal operation: transforms advance as they always have.
    QCOMPARE(harness.controller.spectrumSourceTransformsFollowFrameRate(97),
             std::optional<bool>(false));

    DisplayLoadGovernor governor(ceiling);
    std::optional<DisplayLoadDecision> lowered;
    for (qint64 t = 0; t <= DisplayLoadGovernor::kBusyHoldMs && !lowered; t += 500) {
        DisplayLoadReading reading;
        reading.nowMs = t;
        reading.highestReceiverLoad = 0.8;
        reading.acceptedCharge = harness.controller.acceptedDisplayCharge();
        lowered = governor.update(reading);
    }
    QVERIFY(lowered.has_value());
    QCOMPARE(lowered->reason, DisplayBudgetReason::CoreBusy);
    QVERIFY(lowered->limits.spectrumSampleUnitsPerSecond < requested.spectrumSampleUnitsPerSecond);
    QVERIFY(harness.server.setDisplayBudgetLimits(lowered->limits, lowered->reason));
    governor.accept(*lowered);
    QTRY_COMPARE(harness.client.remoteDisplayBudgetLimits()->generation, quint32{2});
    QCOMPARE(harness.client.remoteDisplayBudgetReason(), DisplayBudgetReason::CoreBusy);
    // R-R3-08/40: while the Core is busy a lower frame rate must save FFT
    // work, so the source's transforms follow its frame rate.
    QCOMPARE(harness.controller.spectrumSourceTransformsFollowFrameRate(97),
             std::optional<bool>(true));

    // Growth no longer fits.
    request.insert(QStringLiteral("revision"), 2);
    request.insert(QStringLiteral("pixels"), 256);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 97, 2).isEmpty());
    QVERIFY(!allocationFor(controls, 97, 2).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(allocationFor(controls, 97, 2).value(QStringLiteral("acceptedRevision")).toInteger(),
             qint64{1});

    // The reduction the app plans under the lowered cap is admitted.
    const DisplayBudgetCharge reduced = spectrumDisplayCost(128, 50, false)->charge;
    QVERIFY(displayChargeFits(lowered->limits, reduced));
    request.insert(QStringLiteral("revision"), 3);
    request.insert(QStringLiteral("pixels"), 128);
    request.insert(QStringLiteral("fps"), 50);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 97, 3).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.acceptedDisplayCharge(), reduced);

    // The step settles on a calm load, then the calm hold restores it.
    std::optional<DisplayLoadDecision> restored;
    for (qint64 t = 10'000;
         t <= 10'000 + DisplayLoadGovernor::kSettleMs + DisplayLoadGovernor::kCalmHoldMs
         && !restored;
         t += 500) {
        DisplayLoadReading reading;
        reading.nowMs = t;
        reading.highestReceiverLoad = 0.3;
        reading.acceptedCharge = harness.controller.acceptedDisplayCharge();
        restored = governor.update(reading);
    }
    QVERIFY(restored.has_value());
    QCOMPARE(restored->reason, DisplayBudgetReason::None);
    QVERIFY(harness.server.setDisplayBudgetLimits(restored->limits, restored->reason));
    governor.accept(*restored);
    QTRY_COMPARE(harness.client.remoteDisplayBudgetLimits()->generation, quint32{3});
    QCOMPARE(harness.client.remoteDisplayBudgetReason(), DisplayBudgetReason::None);
    QCOMPARE(harness.controller.spectrumSourceTransformsFollowFrameRate(97),
             std::optional<bool>(false));
    QCOMPARE(harness.client.remoteDisplayBudgetLimits()->spectrumSampleUnitsPerSecond,
             ceiling.spectrumSampleUnitsPerSecond);

    // The requested quality fits again.
    request.insert(QStringLiteral("revision"), 4);
    request.insert(QStringLiteral("fps"), 60);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 97, 4).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.acceptedDisplayCharge(), requested);
    harness.finish();
}

namespace {

// One deterministic run of eight wide pans at 4096 px asking for 60 fps on
// one source, or split across two (see
// eightWidePansAtTheCeilingGetTheirPlannedFrameRates and
// legacyPansOnTwoSourcesKeepTheirEvenShare).
struct EightPanRun {
    static constexpr int kPans = 8;
    static constexpr int kPixels = 4096;
    static constexpr int kFps = 60;
    static constexpr qint64 kMeasureNs = 10'000'000'000;
    QList<int> fps;       // what each endpoint subscribed at (0: paused)
    QList<int> received;  // spectrum frames sent in the measured window
    int total = 0;        // every spectrum frame in the window
    int ps3Messages = 0;  // PureSignal display chunks in the window
    int admitted = 0;

    double seconds() const { return double(kMeasureNs) / 1e9; }
    QString line() const
    {
        QStringList parts;
        for (int pan = 0; pan < received.size(); ++pan) {
            parts.append(QStringLiteral("%1/%2").arg(received.at(pan) / seconds(), 0, 'f', 1)
                             .arg(fps.at(pan)));
        }
        return QStringLiteral("%1 admitted, spectrum %2 fps, PureSignal %3 chunks/s; "
                              "received/subscribed per pan: %4")
            .arg(admitted).arg(total / seconds(), 0, 'f', 1)
            .arg(ps3Messages / seconds(), 0, 'f', 1).arg(parts.join(QStringLiteral(", ")));
    }
};

// Budget mode subscribes what the app plans under the computed ceiling
// (RemoteDisplayAllocator, with PureSignal's share when it is on), legacy
// mode what the pans ask for. The test drives the source's 60 fps frames,
// PureSignal's 100 ms snapshots and the 5 ms sender ticks on one simulated
// clock, so nothing follows the computer's load. With twoSources the last
// four pans take the stream's fine tier, a second source whose 60 fps frames
// fall a third of a frame after the first source's.
void runEightWidePans(bool budget, bool pureSignal, EightPanRun& result,
                      bool twoSources = false)
{
    constexpr int kPans = EightPanRun::kPans;
    constexpr qint64 kStartNs = 1'000'000'000;
    constexpr qint64 kWarmupNs = 1'000'000'000;
    constexpr qint64 kSourceFps = 60;
    const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
    Harness h(budget ? std::optional<DisplayBudgetLimits>(ceiling) : std::nullopt);
    h.useManualDisplayTicks();
    h.establishSession();
    h.startReadyPeer();
    QList<RemoteDisplayIntent> intents;
    for (int pan = 0; pan < kPans; ++pan) {
        intents.append({QStringLiteral("pan-%1").arg(pan), EightPanRun::kPixels,
                        EightPanRun::kFps, true, 20, pan == 0});
    }
    QList<int> pixels(kPans, EightPanRun::kPixels);
    QList<int> framesPerLine(kPans, 1);
    result.fps = QList<int>(kPans, EightPanRun::kFps);
    if (budget) {
        QString error;
        const auto allocation = allocateRemoteDisplay(ceiling, intents, pureSignal, &error);
        QVERIFY2(allocation.has_value(), qPrintable(error));
        for (const RemoteDisplayQuality& quality : allocation->pans) {
            const int pan = quality.panId.mid(4).toInt();
            result.fps[pan] = quality.suspended ? 0 : quality.fps;
            pixels[pan] = quality.pixels;
            framesPerLine[pan] = quality.framesPerLine;
        }
    }
    PureSignalSessionFacade* const facade = h.radio.pureSignalFacade();
    if (pureSignal) {
        facade->setRemoteAmpViewSubscribed(true);
    }
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    for (int pan = 0; pan < kPans; ++pan) {
        if (result.fps.at(pan) == 0) { continue; }
        const bool second = twoSources && pan >= kPans / 2;
        QJsonObject request = tieredSubscription(
            quint32(pan + 1), 1, h.sliceId, centre,
            second ? QStringLiteral("fine") : QStringLiteral("wide"), EightPanRun::kPixels);
        request.insert(QStringLiteral("pixels"), pixels.at(pan));
        request.insert(QStringLiteral("fps"), result.fps.at(pan));
        request.insert(QStringLiteral("spanHz"), 192000.0);
        request.insert(QStringLiteral("wideSpanFactor"), 2.0);
        request.insert(QStringLiteral("framesPerLine"), framesPerLine.at(pan));
        QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    }
    const int admitted = int(std::count_if(result.fps.cbegin(), result.fps.cend(),
                                           [](int fps) { return fps > 0; }));
    QTRY_COMPARE_WITH_TIMEOUT(h.controller.activeEndpointCount(), admitted, 10'000);
    result.admitted = h.controller.activeEndpointCount();
    auto* source = &h.controller.sharedSpectrum()->source();
    QVERIFY(source);
    const QList<MediaSourceKey> keys = source->activeSources();
    QCOMPARE(keys.size(), twoSources ? 2 : 1);
    const MediaSourceKey key = keys.constFirst();
    // The engine takes its configuration on its own thread; after that
    // every frame comes from here, never from I/Q (none is fed).
    h.nowNs = kStartNs;
    QTRY_VERIFY_WITH_TIMEOUT(source->publishFrameForTest(key, kStartNs), 10'000);
    // The second source's frame k at kStartNs + phase + k / 60 s.
    constexpr qint64 kSecondPhaseNs = 1'000'000'000 / kSourceFps / 3;
    std::optional<MediaSourceKey> secondKey;
    if (twoSources) {
        secondKey = keys.at(1);
        QTRY_VERIFY_WITH_TIMEOUT(
            source->publishFrameForTest(*secondKey, kStartNs + kSecondPhaseNs), 10'000);
    }

    // Source frame k at kStartNs + k / 60 s, PureSignal snapshot n at
    // kStartNs + n x 100 ms, sender tick j at kStartNs + j x 5 ms. Events at
    // the same instant: the frames, then the snapshot, then the tick.
    const qint64 endNs = kStartNs + kWarmupNs + EightPanRun::kMeasureNs;
    const quint64 generation = facade->displayGeneration();
    qint64 frame = 1;
    qint64 secondFrame = 1;
    qint64 snapshot = 0;
    qint64 tick = 1;
    qsizetype first = -1;
    for (;;) {
        const qint64 frameNs = kStartNs + frame * 1'000'000'000 / kSourceFps;
        const qint64 secondFrameNs = secondKey
            ? kStartNs + kSecondPhaseNs + secondFrame * 1'000'000'000 / kSourceFps
            : std::numeric_limits<qint64>::max();
        const qint64 snapshotNs = pureSignal ? kStartNs + snapshot * kPs3DisplayPollIntervalNs
                                             : std::numeric_limits<qint64>::max();
        const qint64 tickNs = kStartNs + tick * kDisplaySenderIntervalNs;
        const qint64 nowNs = std::min({frameNs, secondFrameNs, snapshotNs, tickNs});
        if (nowNs >= endNs) { break; }
        if (first < 0 && nowNs >= kStartNs + kWarmupNs) {
            first = h.mediaTransport->displays.size();
        }
        h.nowNs = nowNs;
        if (frameNs == nowNs) {
            QVERIFY(source->publishFrameForTest(key, frameNs));
            ++frame;
        } else if (secondFrameNs == nowNs) {
            QVERIFY(source->publishFrameForTest(*secondKey, secondFrameNs));
            ++secondFrame;
        } else if (snapshotNs == nowNs) {
            facade->displaySnapshotReady(maximumPs3Snapshot(generation, quint64(snapshot + 1)));
            ++snapshot;
        } else {
            h.sendDisplayTick();
            ++tick;
        }
    }
    result.received = QList<int>(kPans, 0);
    for (qsizetype i = first; i < h.mediaTransport->displays.size(); ++i) {
        const QByteArray& bytes = h.mediaTransport->displays.at(i);
        if (bytes.startsWith(QByteArrayLiteral("PS3D"))) {
            ++result.ps3Messages;
            continue;
        }
        if (bytes.size() < 12) { continue; }
        const quint32 endpoint = qFromBigEndian<quint32>(bytes.constData() + 8);
        if (endpoint >= 1 && endpoint <= quint32(kPans)) { ++result.received[int(endpoint - 1)]; }
    }
    for (int count : std::as_const(result.received)) { result.total += count; }
    h.finish();
}

} // namespace

// R-R3-08, R-R3-37: at the computed ceiling, eight pans at 4096 px, 60 fps
// with the wide plane get what the app plans for them. The app plans exactly
// the sender's 200 messages a second (the active pan at 60, the other seven
// at 20), so the sender must send each endpoint before the frame it holds
// stops being worth sending: earliest deadline first, the active pan first
// on a tie. Deterministic (runEightWidePans). Legacy mode (no budget) is
// measured alongside and keeps its even share.
void TstDaemonMediaController::eightWidePansAtTheCeilingGetTheirPlannedFrameRates()
{
    constexpr int kPans = EightPanRun::kPans;
    EightPanRun legacy;
    runEightWidePans(false, false, legacy);
    if (QTest::currentTestFailed()) { return; }
    EightPanRun budget;
    runEightWidePans(true, false, budget);
    if (QTest::currentTestFailed()) { return; }
    const double seconds = budget.seconds();
    qInfo().noquote() << "legacy:" << legacy.line();
    qInfo().noquote() << "ceiling:" << budget.line();
    QCOMPARE(legacy.admitted, kPans);
    QCOMPARE(budget.admitted, kPans);
    // Legacy mode asks for 480 frames a second: the sender stays full and
    // every pan keeps an even share (none is starved by the ordering).
    for (int pan = 0; pan < kPans; ++pan) {
        QVERIFY2(legacy.received.at(pan)
                     >= int(kDisplaySenderMessagesPerSecond / kPans * seconds) - 1,
                 qPrintable(legacy.line()));
    }
    // The app plans the active pan (pan-0) at the full 60 fps.
    QCOMPARE(budget.fps.at(0), EightPanRun::kFps);
    // The active pan receives its planned rate: every one of its frames in
    // the window, allowing one at the window's edge. Each background pan
    // receives its planned rate the same way.
    for (int pan = 0; pan < kPans; ++pan) {
        QVERIFY2(budget.received.at(pan) >= int(budget.fps.at(pan) * seconds) - 1,
                 qPrintable(budget.line()));
    }
    // At least 195 of the sender's 200 messages a second.
    QVERIFY2(budget.total >= int(195 * seconds), qPrintable(budget.line()));
    QVERIFY(budget.total <= int(kDisplaySenderMessagesPerSecond * seconds));
}

// R-R3-08, R-R3-37: PureSignal's display keeps its share while spectrum
// fills the sender. The same eight pans, with the AmpView open: the app
// plans spectrum beside PureSignal's reserved share, and every 100 ms
// snapshot still goes out whole while the active pan keeps its 60 fps.
void TstDaemonMediaController::pureSignalKeepsItsDisplayShareWhileSpectrumIsContended()
{
    constexpr int kPans = EightPanRun::kPans;
    EightPanRun run;
    runEightWidePans(true, true, run);
    if (QTest::currentTestFailed()) { return; }
    qInfo().noquote() << "ceiling with PureSignal:" << run.line();
    const double seconds = run.seconds();
    QCOMPARE(run.admitted, kPans);
    // Spectrum wants more than the sender has left beside PureSignal.
    const int asked = kPans * EightPanRun::kFps;
    QVERIFY(asked + int(ps3DisplayCharge().messagesPerSecond)
            > int(kDisplaySenderMessagesPerSecond));
    // PureSignal: every chunk of every snapshot in the window, allowing one
    // snapshot at the window's edge.
    const int ps3PerSecond = int(ps3DisplayCharge().messagesPerSecond);
    const int chunksPerSnapshot = ps3PerSecond * int(kPs3DisplayPollIntervalMs) / 1000;
    QVERIFY2(run.ps3Messages >= int(ps3PerSecond * seconds) - chunksPerSnapshot,
             qPrintable(run.line()));
    // Spectrum still gets what the app planned beside it.
    QCOMPARE(run.fps.at(0), EightPanRun::kFps);
    for (int pan = 0; pan < kPans; ++pan) {
        QVERIFY2(run.received.at(pan) >= int(run.fps.at(pan) * seconds) - 1,
                 qPrintable(run.line()));
    }
}

// R-R3-08, R-R3-37: an older app (no display budget) asks for more than
// the sender can carry, and the Core has no plan to protect, so every pan
// keeps an even share whichever receiver it shows. Eight pans at 60 fps,
// four on each of two sources whose frames fall at different instants:
// 480 frames a second asked of the sender's 200, so 25 fps each.
void TstDaemonMediaController::legacyPansOnTwoSourcesKeepTheirEvenShare()
{
    constexpr int kPans = EightPanRun::kPans;
    EightPanRun legacy;
    runEightWidePans(false, false, legacy, true);
    if (QTest::currentTestFailed()) { return; }
    qInfo().noquote() << "legacy, two sources:" << legacy.line();
    const double seconds = legacy.seconds();
    QCOMPARE(legacy.admitted, kPans);
    for (int pan = 0; pan < kPans; ++pan) {
        QVERIFY2(legacy.received.at(pan)
                     >= int(kDisplaySenderMessagesPerSecond / kPans * seconds) - 1,
                 qPrintable(legacy.line()));
    }
    QVERIFY2(legacy.total >= int(195 * seconds), qPrintable(legacy.line()));
    QVERIFY(legacy.total <= int(kDisplaySenderMessagesPerSecond * seconds));
}

void TstDaemonMediaController::failedDisplayAttemptDebitsCreditAndRecoversWithKeyframe()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(4096, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 19});
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(96, 1, harness.sliceId, centre);
    request.insert(QStringLiteral("pixels"), 4096);
    // Include enough source bins that the first actual frame exhausts the
    // remaining credit needed for another worst-case preflight. The budget
    // charges granted pixels, so the FFT must supply all 4096 of them.
    request.insert(QStringLiteral("spanHz"), 192000.0);
    request.insert(QStringLiteral("fftSize"), 4096);
    request.insert(QStringLiteral("fps"), 60);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 96, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 96)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());

    bool reject = true;
    harness.mediaTransport->onDisplaySend = [&reject] { return !reject; };
    harness.sendDisplayTick();
    QTRY_COMPARE(harness.mediaTransport->displays.size(), 1);
    reject = false;

    request.insert(QStringLiteral("revision"), 2);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 96, 2).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.1875);
        return messageFor(controls, QStringLiteral("context"), 96)
            .value(QStringLiteral("revision")).toInteger() == 2;
    })());
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1); // failed attempt was not refunded

    harness.nowNs = 1'000'000'000;
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.21875);
        harness.sendDisplayTick();
        return harness.mediaTransport->displays.size() == 2;
    })());
    DisplayCodecDecoder decoder;
    const DisplayCodecDecodeResult recovered = decoder.decode(
        harness.mediaTransport->displays.constLast());
    QCOMPARE(recovered.disposition, DisplayCodecDisposition::Accepted);
    harness.finish();
}

// Control logging lane: the media connection's selected pair (candidate
// types and transports, masked addresses) is logged once it is known, not
// again while unchanged, and again on a change seen by the periodic
// diagnostics check; each line carries the connection's rtt.
void TstDaemonMediaController::theMediaPairIsLoggedWhenKnownAndOnChange()
{
    g_daemonMediaMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureDaemonMediaMessages);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    Harness harness;
    harness.establishSession();
    QVERIFY(harness.client.mediaAvailable());
    const QJsonObject start{
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}};
    QVERIFY(harness.client.sendMediaControl(start, harness.client.sessionEpoch()));
    QTRY_VERIFY(harness.mediaTransport);
    MediaIcePath path;
    path.localType = QStringLiteral("host");
    path.localTransport = QStringLiteral("udp");
    path.remoteType = QStringLiteral("srflx");
    path.remoteTransport = QStringLiteral("udp");
    path.localAddress = QStringLiteral("192.168.1.10");
    path.localPort = 5000;
    path.remoteAddress = QStringLiteral("10.0.0.77");
    path.remotePort = 6000;
    harness.mediaTransport->path = path;
    harness.mediaTransport->rtt = 12;
    harness.mediaTransport->becomeReady();
    const auto links = [] {
        return g_daemonMediaMessages.filter(QStringLiteral("Media link for "));
    };
    QTRY_COMPARE(links().size(), 1);
    QVERIFY2(QRegularExpression(QStringLiteral(
                 "^Media link for [0-9a-f]+: direct pair, candidates local host udp, remote "
                 "srflx udp, local \\*\\.\\*\\.\\*\\. 10 port 5000, remote "
                 "\\*\\.\\*\\.\\*\\. 77 port 6000; rtt 12 ms$"))
                 .match(links().constFirst())
                 .hasMatch(),
             qPrintable(links().constFirst()));

    // The 10 s diagnostics timer, fired here by hand.
    QTimer* diagnostics = nullptr;
    for (QTimer* timer : harness.controller.findChildren<QTimer*>(
             QString(), Qt::FindDirectChildrenOnly)) {
        if (timer->interval() == 10'000) {
            diagnostics = timer;
        }
    }
    QVERIFY(diagnostics);
    const auto check = [diagnostics] {
        return QMetaObject::invokeMethod(diagnostics, "timeout", Qt::DirectConnection);
    };

    // Unchanged: the check logs nothing more.
    QVERIFY(check());
    QCOMPARE(links().size(), 1);

    // A new pair (now through a TURN relay over TCP): logged as a change.
    path.localType = QStringLiteral("relay");
    path.localTransport = QStringLiteral("tcp-active");
    harness.mediaTransport->path = path;
    harness.mediaTransport->rtt = 48;
    QVERIFY(check());
    QCOMPARE(links().size(), 2);
    QVERIFY2(QRegularExpression(QStringLiteral(
                 "^Media link for [0-9a-f]+ \\(changed\\): relayed pair, candidates local "
                 "relay tcp-active, remote srflx udp, .*; rtt 48 ms$"))
                 .match(links().constLast())
                 .hasMatch(),
             qPrintable(links().constLast()));
    harness.finish();
}

// Control logging lane: with the "tx" channel open, a keepalive checks
// the pair, at most once a second; the keepalive still reaches the Core.
void TstDaemonMediaController::aTxKeepaliveChecksTheMediaPairAtMostOnceASecond()
{
    g_daemonMediaMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureDaemonMediaMessages);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    Harness harness;
    harness.establishSession();
    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("remoteTxVersion"), 1}},
        harness.client.sessionEpoch()));
    QTRY_VERIFY(harness.mediaTransport);
    QVERIFY(harness.mediaTransport->startOptions.micAudioSsrc != 0);
    MediaIcePath path;
    path.localType = QStringLiteral("host");
    path.localTransport = QStringLiteral("udp");
    path.remoteType = QStringLiteral("host");
    path.remoteTransport = QStringLiteral("udp");
    path.localAddress = QStringLiteral("192.168.1.10");
    path.localPort = 5000;
    path.remoteAddress = QStringLiteral("192.168.1.20");
    path.remotePort = 6000;
    harness.mediaTransport->path = path;
    harness.mediaTransport->becomeReady();
    const auto links = [] {
        return g_daemonMediaMessages.filter(QStringLiteral("Media link for "));
    };
    QTRY_COMPARE(links().size(), 1);
    QVERIFY2(links().constFirst().endsWith(QStringLiteral("; rtt not measured")),
             qPrintable(links().constFirst()));

    // Changed at once: a keepalive within the second does not look.
    path.remoteType = QStringLiteral("srflx");
    path.remoteAddress = QStringLiteral("10.0.0.77");
    harness.mediaTransport->path = path;
    emit harness.mediaTransport->txReceived(QByteArrayLiteral("x"), 0);
    QCoreApplication::processEvents();
    QCOMPARE(links().size(), 1);

    // After the second, the next keepalive does.
    QTest::qWait(1100);
    emit harness.mediaTransport->txReceived(QByteArrayLiteral("x"), 0);
    QTRY_COMPARE(links().size(), 2);
    QVERIFY2(links().constLast().contains(QStringLiteral(
                 " (changed): direct pair, candidates local host udp, remote srflx udp, ")),
             qPrintable(links().constLast()));
    harness.finish();
}

void TstDaemonMediaController::displayDiagnosticsMeasureSentFramesRefusalsAndErrors()
{
    g_daemonMediaMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureDaemonMediaMessages);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    Harness harness;
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QCOMPARE(harness.controller.displayDiagnostics(), DaemonDisplayDiagnostics{});
    QJsonObject request = subscription(
        61, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    request.insert(QStringLiteral("pixels"), 1024);
    request.insert(QStringLiteral("spanHz"), 192000.0);
    request.insert(QStringLiteral("fftSize"), 4096);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 61).isEmpty();
    })());

    int cycle = 0;
    const auto sendOneFrame = [&harness, &cycle] {
        const qsizetype before = harness.mediaTransport->displays.size();
        QTRY_VERIFY(([&] {
            harness.feedRadio(0.125 + 0.0078125 * (++cycle % 16));
            harness.sendDisplayTick();
            return harness.mediaTransport->displays.size() == before + 1;
        })());
    };
    QList<QByteArray> accepted;
    for (int i = 0; i < 4; ++i) {
        sendOneFrame();
        accepted.append(harness.mediaTransport->displays.constLast());
    }

    // 1024/1024 points, no 3D row: 42 + 2 * (3 + 5 * 8 + 1024) = 2176 bytes.
    QVERIFY(isKeyframe(accepted.constFirst()));
    QCOMPARE(accepted.constFirst().size(),
             int(kDisplayCodecHeaderBytes + 2 * displayCodecWorstCasePlaneBytes(1024)));
    QCOMPARE(accepted.constFirst().size(), 2176);
    quint32 largestDelta = 0;
    for (qsizetype i = 1; i < accepted.size(); ++i) {
        QVERIFY(!isKeyframe(accepted.at(i)));
        QVERIFY(accepted.at(i).size() <= accepted.constFirst().size());
        largestDelta = std::max(largestDelta, quint32(accepted.at(i).size()));
    }
    DaemonDisplayDiagnostics diagnostics = harness.controller.displayDiagnostics();
    QCOMPARE(diagnostics.displayMaxKeyframeBytes, quint32(2176));
    QCOMPARE(diagnostics.displayMaxDeltaBytes, largestDelta);
    QCOMPARE(diagnostics.displayMaxFragments, quint32(3)); // ceil(2176 / 876)
    QCOMPARE(diagnostics.displaySendRefusals, quint64(0));
    QCOMPARE(diagnostics.displayTransportErrors, quint64(0));

    // A full transport refuses the frame: counted, never resent, never
    // measured, and the next frame that goes is a keyframe.
    harness.mediaTransport->onDisplaySend = [] { return false; };
    sendOneFrame();
    harness.mediaTransport->onDisplaySend = {};
    QCOMPARE(harness.controller.displayDiagnostics().displaySendRefusals, quint64(1));
    sendOneFrame();
    QVERIFY(isKeyframe(harness.mediaTransport->displays.constLast()));
    sendOneFrame();
    QVERIFY(!isKeyframe(harness.mediaTransport->displays.constLast()));

    // A frame the library takes but holds until SCTP has room is still
    // delivered: measured, counted as queued late, not refused, and the
    // delta chain goes on.
    harness.mediaTransport->nextSubmitResult = IMediaTransport::DisplaySendResult::Queued;
    sendOneFrame();
    QVERIFY(!isKeyframe(harness.mediaTransport->displays.constLast()));
    QCOMPARE(harness.controller.displayDiagnostics().displayQueuedLate, quint64(1));
    QCOMPARE(harness.controller.displayDiagnostics().displaySendRefusals, quint64(1));
    sendOneFrame();
    QVERIFY(!isKeyframe(harness.mediaTransport->displays.constLast()));

    // A media error that is not about the display channel (signalling, the
    // transport factory, audio) is logged once but is not a display error.
    emit harness.mediaTransport->errorOccurred(QStringLiteral("remote candidate rejected"));
    emit harness.mediaTransport->errorOccurred(QStringLiteral("remote candidate rejected"));
    QCOMPARE(harness.controller.displayDiagnostics().displayTransportErrors, quint64(0));
    QCOMPARE(g_daemonMediaMessages.filter(QStringLiteral("remote candidate rejected")).size(), 1);
    sendOneFrame();
    QVERIFY(!isKeyframe(harness.mediaTransport->displays.constLast()));

    // Display-channel errors: each counts, each distinct text is logged
    // once, and each forces the next frame to be a keyframe like a failed
    // send.
    emit harness.mediaTransport->displayErrorOccurred(QStringLiteral("sctp send failed"));
    emit harness.mediaTransport->displayErrorOccurred(QStringLiteral("sctp send failed"));
    emit harness.mediaTransport->displayErrorOccurred(QStringLiteral("dtls record rejected"));
    diagnostics = harness.controller.displayDiagnostics();
    QCOMPARE(diagnostics.displayTransportErrors, quint64(3));
    QCOMPARE(diagnostics.displaySendRefusals, quint64(1));
    QCOMPARE(g_daemonMediaMessages.filter(QStringLiteral("media transport error:")).size(), 2);
    QCOMPARE(g_daemonMediaMessages.filter(QStringLiteral("sctp send failed")).size(), 1);
    sendOneFrame();
    QVERIFY(isKeyframe(harness.mediaTransport->displays.constLast()));
    QCOMPARE(harness.controller.displayDiagnostics().displayMaxKeyframeBytes, quint32(2176));

    // Retiring the peer writes the final line; a new peer starts from zero.
    harness.mediaTransport->onDisplaySend = [transport = harness.mediaTransport] {
        emit transport->closed();
        return true;
    };
    sendOneFrame();
    QTRY_VERIFY(harness.mediaTransport.isNull());
    const QStringList finals =
        g_daemonMediaMessages.filter(QStringLiteral("daemon display diagnostics final"));
    QCOMPARE(finals.size(), 1);
    QVERIFY2(finals.constFirst().contains(
                 QStringLiteral("largestKeyframe=2176 bytes/3 fragments")),
             qPrintable(finals.constFirst()));
    QVERIFY(finals.constFirst().contains(QStringLiteral("transportErrors=3")));
    // Control logging lane: the media connection's rtt rides along.
    QVERIFY2(finals.constFirst().contains(QStringLiteral(" mediaRttMs=")),
             qPrintable(finals.constFirst()));
    harness.startReadyPeer();
    QCOMPARE(harness.controller.displayDiagnostics(), DaemonDisplayDiagnostics{});
    harness.finish();
}

// R-R3-03/R-R3-05: a display error from the real libdatachannel transport,
// through MediaPeer, reaches Core once: counted and logged as a display
// error, never also logged as a media peer error.
void TstDaemonMediaController::realDisplayErrorIsCountedAndLoggedOnce()
{
    g_daemonMediaMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureDaemonMediaMessages);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    Harness harness;
    QPointer<LibDataChannelMediaTransport> core;
    harness.realTransport = [&core](QObject* parent) -> IMediaTransport* {
        core = new LibDataChannelMediaTransport(parent);
        return core;
    };
    harness.useManualDisplayTicks();
    harness.establishSession();

    // The far end answers Core's offer over the session, as a GUI does.
    LibDataChannelMediaTransport far;
    QSignalSpy farDisplays(&far, &IMediaTransport::displayReceived);
    QSignalSpy farFailed(&far, &IMediaTransport::connectionFailed);
    QVERIFY(far.start({IMediaTransport::Role::Answerer, 0x4e523302U}));
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    connect(&harness.client, &StationClient::mediaControlReceived, &far,
            [&far](const QJsonObject& control) {
        const QString op = control.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("description")) {
            QVERIFY(far.acceptDescription(control.value(QStringLiteral("sdp")).toString(),
                                          control.value(QStringLiteral("type")).toString()));
        } else if (op == QLatin1String("candidate")) {
            QVERIFY(far.acceptCandidate(control.value(QStringLiteral("candidate")).toString(),
                                        control.value(QStringLiteral("mid")).toString()));
        }
    });
    connect(&far, &IMediaTransport::localDescription, &harness.client,
            [&harness](const QString& sdp, const QString& type) {
        QVERIFY(harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("description")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("sdp"), sdp},
            {QStringLiteral("type"), type}}, harness.client.sessionEpoch()));
    });
    connect(&far, &IMediaTransport::localCandidate, &harness.client,
            [&harness](const QString& candidate, const QString& mid) {
        QVERIFY(harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("candidate")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("candidate"), candidate},
            {QStringLiteral("mid"), mid}}, harness.client.sessionEpoch()));
    });
    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
        harness.client.sessionEpoch()));
    QTRY_VERIFY2_WITH_TIMEOUT(core && core->isReady() && far.isReady(),
                              farFailed.isEmpty() ? "the media link did not come up in time"
                                                  : "the far end's media link failed",
                              kRealTransportWaitMs);

    QVERIFY(harness.client.sendMediaControl(
        subscription(62, 1, harness.sliceId,
                     harness.radio.streamCentreHz(harness.streamIndex)),
        harness.client.sessionEpoch()));
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        harness.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 62).isEmpty();
    })(), kRealTransportWaitMs);
    int cycle = 0;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        harness.feedRadio(0.125 + 0.0078125 * (++cycle % 16));
        harness.sendDisplayTick();
        return !farDisplays.isEmpty();
    })(), kRealTransportWaitMs);
    // Every frame Core has sent so far was produced before this point.
    QElapsedTimer sinceLastSend;
    sinceLastSend.start();
    QCOMPARE(harness.controller.displayDiagnostics().displayTransportErrors, quint64(0));

    // A frame waits to be sent. The far end goes away; before Core's event
    // loop hears of it, the next display send finds the channel closed and
    // libdatachannel throws.
    //
    // The send needs three things, and each is waited on rather than
    // assumed after a fixed delay, which a loaded machine outruns:
    //  - the channel holds no earlier message (a busy one is never offered
    //    a frame);
    //  - the frame is due: the endpoint drops a frame produced within one
    //    output period (60 fps, plus 5% early tolerance) of the last one it
    //    sent, so the frame must be published more than that after the
    //    last send;
    //  - Core holds that frame as its latest input. frameAvailable is
    //    emitted on this thread and Core takes the source's latest frame
    //    inside the emit, so an emit after a newer frame was published
    //    hands Core a due frame. No display tick runs until the one below.
    QTRY_VERIFY_WITH_TIMEOUT(!core->displayBusy(), kRealTransportWaitMs);
    constexpr qint64 kOutputPeriodMs = 1000 / 60 + 1;
    QTRY_VERIFY(sinceLastSend.elapsed() > 2 * kOutputPeriodMs);
    auto* source = &harness.controller.sharedSpectrum()->source();
    QVERIFY(source);
    const QList<MediaSourceKey> sourceKeys = source->activeSources();
    QCOMPARE(sourceKeys.size(), 1);
    const MediaSourceKey sourceKey = sourceKeys.constFirst();
    QSignalSpy sourceFrames(source, &DaemonSpectrumSource::frameAvailable);
    const quint64 publishedBefore = source->publishedFrames(sourceKey);
    const quint64 submitted = core->telemetry()->submittedDisplayPayloadBytes;
    bool duePublished = false;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        if (!duePublished && source->publishedFrames(sourceKey) > publishedBefore) {
            // An emit already seen may have taken an older frame.
            duePublished = true;
            sourceFrames.clear();
        }
        if (duePublished && !sourceFrames.isEmpty()) {
            return true;
        }
        harness.feedRadio(0.3125);
        return false;
    })(), kRealTransportWaitMs);
    far.stop();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    harness.sendDisplayTick();
    QVERIFY2(core, "Core retired its transport before the send");
    QVERIFY2(core->telemetry()->submittedDisplayPayloadBytes > submitted,
             "no display frame was waiting to be sent");

    QCOMPARE(harness.controller.displayDiagnostics().displayTransportErrors, quint64(1));
    const QStringList displayErrors =
        g_daemonMediaMessages.filter(QStringLiteral("media transport error:"));
    QCOMPARE(displayErrors.size(), 1);
    QVERIFY2(displayErrors.constFirst().contains(QStringLiteral("DataChannel")),
             qPrintable(displayErrors.constFirst()));
    QCOMPARE(g_daemonMediaMessages.filter(QStringLiteral("media peer error:")).size(), 0);
    harness.finish();
}

// TX mic thread fix round 2: the Core's microphone line on the real
// transport, its packets handed to the receiver on the transport's own
// thread (NereusMicRx) while the phone keeps sending, is torn down from the
// owner: by a start on a new connection (the session stays, the peer and
// its line go) and by the session's end. Delivery stops with the line, the
// receiver goes, and nothing reaches a receiver after it is gone (run under
// ThreadSanitizer for the races).
void TstDaemonMediaController::realMicLineTornDownWhileItsThreadDelivers_data()
{
    QTest::addColumn<bool>("sessionEnds");
    QTest::newRow("a new connection's start") << false;
    QTest::newRow("the session's end") << true;
}

void TstDaemonMediaController::realMicLineTornDownWhileItsThreadDelivers()
{
    QFETCH(bool, sessionEnds);
    RemoteMicEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness harness;
    QPointer<LibDataChannelMediaTransport> core;
    // TX watch follow-up: packets the transport hands to its owner instead
    // of the line's sink (on the owner's thread).
    int ownerMicPackets = 0;
    harness.realTransport = [&core, &ownerMicPackets](QObject* parent) -> IMediaTransport* {
        core = new LibDataChannelMediaTransport(parent);
        connect(core.data(), &IMediaTransport::micRtpReceived, core.data(),
                [&ownerMicPackets](const QByteArray&, qint64) { ++ownerMicPackets; });
        return core;
    };
    harness.establishSession();

    // The phone: its transport on a thread of its own, answering Core's
    // offer over the session.
    QThread phoneThread;
    phoneThread.setObjectName(QStringLiteral("TestPhone"));
    phoneThread.start();
    QObject phoneContext;
    phoneContext.moveToThread(&phoneThread);
    std::unique_ptr<LibDataChannelMediaTransport> far;
    const auto onPhone = [&phoneContext](auto work) {
        QMetaObject::invokeMethod(&phoneContext, work, Qt::BlockingQueuedConnection);
    };
    const auto endPhone = qScopeGuard([&]() {
        onPhone([&far]() { far.reset(); });
        phoneThread.quit();
        phoneThread.wait();
    });
    onPhone([&far]() { far = std::make_unique<LibDataChannelMediaTransport>(); });
    const quint32 micSsrc = MediaPeer::micAudioSsrcForConnection(QLatin1String(kConnectionId));
    std::atomic<bool> farReady{false};
    std::atomic<bool> farFailed{false};
    connect(far.get(), &IMediaTransport::ready, &phoneContext,
            [&farReady]() { farReady.store(true); });
    connect(far.get(), &IMediaTransport::connectionFailed, &phoneContext,
            [&farFailed](const QString&) { farFailed.store(true); });
    bool farStarted = false;
    onPhone([&]() {
        IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer, 0x4e523302U};
        options.micAudioSsrc = micSsrc;
        farStarted = far->start(options);
    });
    QVERIFY(farStarted);
    connect(&harness.client, &StationClient::mediaControlReceived, &harness.client,
            [&](const QJsonObject& control) {
        const QString op = control.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("description")) {
            onPhone([&far, control]() {
                far->acceptDescription(control.value(QStringLiteral("sdp")).toString(),
                                       control.value(QStringLiteral("type")).toString());
            });
        } else if (op == QLatin1String("candidate")) {
            onPhone([&far, control]() {
                far->acceptCandidate(control.value(QStringLiteral("candidate")).toString(),
                                     control.value(QStringLiteral("mid")).toString());
            });
        }
    });
    connect(far.get(), &IMediaTransport::localDescription, &harness.client,
            [&harness](const QString& sdp, const QString& type) {
        harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("description")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("sdp"), sdp},
            {QStringLiteral("type"), type}}, harness.client.sessionEpoch());
    });
    connect(far.get(), &IMediaTransport::localCandidate, &harness.client,
            [&harness](const QString& candidate, const QString& mid) {
        harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("candidate")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("candidate"), candidate},
            {QStringLiteral("mid"), mid}}, harness.client.sessionEpoch());
    });
    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("remoteTxVersion"), 1}},
        harness.client.sessionEpoch()));
    QTRY_VERIFY2_WITH_TIMEOUT(core && core->isReady() && farReady.load(),
                              farFailed.load() ? "the phone's media link failed"
                                               : "the media link did not come up in time",
                              kRealTransportWaitMs);
    QVERIFY2(harness.controller.micReceiver() != nullptr, "the Core opened no microphone line");

    // The phone's microphone: a 20 ms packet every 2 ms, so the line's
    // thread is delivering whenever the owner tears it down.
    std::vector<QByteArray> packets;
    {
        std::vector<float> frame(RemoteMicConfig::kOpusFrameSamples, 0.0f);
        for (int k = 0; k < 2000; ++k) {
            for (int i = 0; i < RemoteMicConfig::kOpusFrameSamples; ++i) {
                frame[static_cast<size_t>(i)] = 0.2f * static_cast<float>(std::sin(
                    2.0 * std::numbers::pi * 1000.0
                    * (static_cast<double>(k) * RemoteMicConfig::kOpusFrameSamples + i) / 48000.0));
            }
            packets.push_back(encoder.encode(frame.data(), static_cast<quint16>(k + 1),
                                             static_cast<quint32>(k)
                                                 * RemoteMicConfig::kOpusFrameSamples,
                                             micSsrc));
        }
    }
    std::atomic<bool> stopSending{false};
    std::atomic<int> sent{0};   // packets handed to the phone's transport
    std::thread sender([&]() {
        for (size_t k = 0; k < packets.size() && !stopSending.load(); ++k) {
            const QByteArray packet = packets[k];
            QMetaObject::invokeMethod(
                &phoneContext,
                [&far, &sent, packet]() {
                    if (far) {
                        far->sendMicRtp(packet);
                        sent.fetch_add(1);
                    }
                },
                Qt::QueuedConnection);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    const auto joinSender = qScopeGuard([&]() {
        stopSending.store(true);
        sender.join();
    });
    harness.recordClockThreads.store(true);
    QTRY_VERIFY_WITH_TIMEOUT(harness.controller.micReceiver() != nullptr
                                 && harness.controller.micReceiver()->stats().decodedPackets >= 20,
                             kRealTransportWaitMs);
    // TX watch follow-up: the packets came on the line's own thread, never
    // through the owner.
    harness.recordClockThreads.store(false);
    {
        const std::lock_guard<std::mutex> lock(harness.clockThreadsLock);
        QVERIFY2(harness.clockThreads.contains(QStringLiteral("NereusMicRx")),
                 qPrintable(QStringList(harness.clockThreads.values()).join(QLatin1Char(','))));
    }
    QCOMPARE(ownerMicPackets, 0);
    // The line being torn down (a new connection's start makes another).
    const QPointer<LibDataChannelMediaTransport> lineCore = core;

    // Torn down from the owner while the line's thread delivers.
    if (sessionEnds) {
        harness.finish();
        QTRY_VERIFY(!harness.server.mediaAvailable());
    } else {
        QVERIFY(harness.client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("start")},
            {QStringLiteral("connectionId"),
             QStringLiteral("22222222-3333-4444-8555-666666666666")},
            {QStringLiteral("remoteTxVersion"), 1}},
            harness.client.sessionEpoch()));
        // The old connection's transport is stopped (its peer goes later).
        QTRY_VERIFY(!lineCore || !lineCore->isReady());
    }
    QTRY_VERIFY(harness.controller.micReceiver() == nullptr
                || harness.controller.micReceiver()->stats().decodedPackets == 0);
    // The phone goes on sending to a line that is gone.
    QTest::qWait(200);
    QVERIFY(sent.load() > 20);
    // A new connection's line (not yet up) has heard nothing of the old:
    // nothing accepted, nothing rejected, nothing decoded. The session's
    // end leaves no line; a new connection's start has one.
    const RemoteMicReceiver* receiver = harness.controller.micReceiver();
    if (!sessionEnds) {
        QVERIFY2(receiver != nullptr, "the new connection opened no microphone line");
    }
    if (receiver != nullptr) {
        const RemoteMicReceiver::Stats stats = receiver->stats();
        QCOMPARE(stats.accepted, quint64(0));
        QCOMPARE(stats.rejectedPackets, quint64(0));
        QCOMPARE(stats.decodedPackets, quint64(0));
    }
    QCOMPARE(ownerMicPackets, 0);
}

void TstDaemonMediaController::displayDiagnosticsLineReportsBytesAndFragments()
{
    DaemonDisplayDiagnostics small;
    small.displayMaxKeyframeBytes = 2977; // 1024/1024 points with a 768-point 3D row
    small.displayMaxDeltaBytes = 1800;
    small.displayMaxFragments = 4;
    QCOMPARE(daemonDisplayDiagnosticsLine(small),
             QStringLiteral("largestKeyframe=2977 bytes/4 fragments"
                            " largestDelta=1800 bytes/3 fragments"
                            " maxFragments=4 sendRefusals=0 transportErrors=0"
                            " queuedLate=0 keyframeRequests=0"
                            " keyframeRequestsRefused=0"));

    DaemonDisplayDiagnostics largest;
    largest.displayMaxKeyframeBytes = 9361; // 4096/4096 points with a 768-point 3D row
    largest.displayMaxDeltaBytes = 9361;
    largest.displayMaxFragments = 11;
    largest.displaySendRefusals = 7;
    largest.displayTransportErrors = 2;
    largest.displayQueuedLate = 3;
    largest.displayKeyframeRequests = 12;
    largest.displayKeyframeRequestsRefused = 1;
    QCOMPARE(daemonDisplayDiagnosticsLine(largest),
             QStringLiteral("largestKeyframe=9361 bytes/11 fragments"
                            " largestDelta=9361 bytes/11 fragments"
                            " maxFragments=11 sendRefusals=7 transportErrors=2"
                            " queuedLate=3 keyframeRequests=12"
                            " keyframeRequestsRefused=1"));
    QCOMPARE(IMediaTransport::sctpFragmentCount(876), quint64(1));
    QCOMPARE(IMediaTransport::sctpFragmentCount(877), quint64(2));
    QCOMPARE(IMediaTransport::sctpFragmentCount(65536), quint64(75));
}

void TstDaemonMediaController::exhaustedDisplayCreditDoesNotBlockAudioRtp()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    const DisplayBudgetCharge charge = ps3DisplayCharge();
    Harness harness(DisplayBudgetLimits{charge.applicationBytesPerSecond, 1, 21});
    AudioEngine* const engine = harness.radio.audioEngine();
    QVERIFY(engine);
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(harness.sliceId, true);
    engine->setSliceStreaming(harness.spareSliceId, true);
    harness.useManualDisplayTicks();
    harness.establishSession();
    harness.startReadyPeer();

    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    facade->displaySnapshotReady(maximumPs3Snapshot(facade->displayGeneration(), 1));
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1);
    QCOMPARE(harness.mediaTransport->displays.constFirst().size(),
             static_cast<qsizetype>(Ps3DisplayCodec::kMaxChunkBytes));
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1); // global display bucket exhausted

    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    QVERIFY(harness.client.sendMediaControl(
        audioControl(1, true), harness.client.sessionEpoch()));
    QTRY_VERIFY(messageFor(controls, QStringLiteral("audio-context"), 0)
                    .value(QStringLiteral("enabled")).toBool());
    QVector<float> left(64 * 2, 0.20f);
    QVector<float> right(64 * 2, 0.30f);
    for (int delivered = 0; delivered < DaemonAudioSource::kBlockFrames; delivered += 64) {
        engine->rxBlockReady(harness.sliceId, left.constData(), 64);
        engine->rxBlockReady(harness.spareSliceId, right.constData(), 64);
    }
    QTRY_VERIFY(harness.mediaTransport->rtpAttempts > 0);
    QTRY_VERIFY(!harness.mediaTransport->rtpPackets.isEmpty());
    harness.finish();
}

void TstDaemonMediaController::mediaPeerReplacementDoesNotMintDisplayCredit()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(4096, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 22});
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QJsonObject request = subscription(
        97, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    request.insert(QStringLiteral("pixels"), 4096);
    // Include enough source bins that the first actual frame exhausts the
    // remaining credit needed for another worst-case preflight. The budget
    // charges granted pixels, so the FFT must supply all 4096 of them.
    request.insert(QStringLiteral("spanHz"), 192000.0);
    request.insert(QStringLiteral("fftSize"), 4096);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 97, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 97)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());
    harness.mediaTransport->onDisplaySend = [transport = harness.mediaTransport] {
        emit transport->closed();
        return true;
    };
    harness.sendDisplayTick();
    QTRY_VERIFY(harness.mediaTransport.isNull());

    controls.clear();
    harness.startReadyPeer();
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 97, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.1875);
        return messageFor(controls, QStringLiteral("context"), 97)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 0);

    harness.nowNs = 1'000'000'000;
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.21875);
        harness.sendDisplayTick();
        return harness.mediaTransport->displays.size() == 1;
    })());
    harness.finish();
}

void TstDaemonMediaController::mixedSpectrumAndPs3StayWithinInjectedIntervalBoundsAndBothProgress()
{
    const SpectrumDisplayCost spectrum = *spectrumDisplayCost(128, 60, false);
    const DisplayBudgetCharge combined = *sumDisplayCharges(
        {spectrum.charge, ps3DisplayCharge()});
    const DisplayBudgetLimits limits{combined.applicationBytesPerSecond,
                                     combined.spectrumSampleUnitsPerSecond, 24};
    Harness harness(limits);
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QVERIFY(harness.client.sendMediaControl(
        subscription(98, 1, harness.sliceId, centre), harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 98, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 98)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());

    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    facade->displaySnapshotReady(maximumPs3Snapshot(facade->displayGeneration(), 1));

    for (int interval = 0; interval < 8; ++interval) {
        harness.nowNs += 50'000'000;
        harness.feedRadio(0.125 + static_cast<double>(interval) / 1024.0);
        const int before = harness.mediaTransport->displays.size();
        QTRY_VERIFY(([&] {
            harness.feedRadio(0.125 + static_cast<double>(interval) / 1024.0);
            harness.sendDisplayTick();
            return harness.mediaTransport->displays.size() > before;
        })());
    }

    const int spectrumMessages = displayMessageCount(
        harness.mediaTransport->displays, QByteArrayLiteral("NSDC"));
    const int ps3Messages = displayMessageCount(
        harness.mediaTransport->displays, QByteArrayLiteral("PS3D"));
    QVERIFY(spectrumMessages > 0);
    QVERIFY(ps3Messages >= 3);

    quint64 attemptedBytes = 0;
    quint64 attemptedSamples = 0;
    DisplayCodecDecoder decoder;
    for (const QByteArray& bytes : std::as_const(harness.mediaTransport->displays)) {
        attemptedBytes += static_cast<quint64>(bytes.size());
        if (!bytes.startsWith(QByteArrayLiteral("NSDC"))) { continue; }
        const DisplayCodecDecodeResult decoded = decoder.decode(bytes);
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
        attemptedSamples += static_cast<quint64>(decoded.frame.traceDbm.size())
            + static_cast<quint64>(decoded.frame.waterfallDbm.size())
            + static_cast<quint64>(decoded.frame.wideDbm.size());
    }
    const quint64 elapsedNs = static_cast<quint64>(harness.nowNs);
    const quint64 allowedBytes = kMaximumDisplayMessageBytes
        + limits.applicationBytesPerSecond * elapsedNs / 1'000'000'000ULL;
    const quint64 allowedSamples = kMaximumSpectrumDisplayFrameSampleUnits
        + limits.spectrumSampleUnitsPerSecond * elapsedNs / 1'000'000'000ULL;
    QVERIFY(attemptedBytes <= allowedBytes);
    QVERIFY(attemptedSamples <= allowedSamples);
    harness.finish();
}

void TstDaemonMediaController::failedMiddlePs3ChunkDropsRemainderPromotesLatestAndDoesNotRefund()
{
    const DisplayBudgetCharge charge = ps3DisplayCharge();
    Harness harness(DisplayBudgetLimits{charge.applicationBytesPerSecond, 1, 25});
    harness.useManualDisplayTicks();
    harness.establishSession();
    harness.startReadyPeer();
    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    const quint64 generation = facade->displayGeneration();
    const Ps3Snapshot first = maximumPs3Snapshot(generation, 1);
    const Ps3Snapshot latest = maximumPs3Snapshot(generation, 2);
    QCOMPARE(Ps3DisplayCodec::encode(first).size(), 3);
    QCOMPARE(Ps3DisplayCodec::encode(latest).size(), 3);

    facade->displaySnapshotReady(first);
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1);
    facade->displaySnapshotReady(latest);

    bool rejectNext = true;
    harness.mediaTransport->onDisplaySend = [&rejectNext] {
        const bool accepted = !rejectNext;
        rejectNext = false;
        return accepted;
    };
    harness.nowNs += 50'000'000;
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 2);
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 2); // failed bytes were not refunded
    harness.mediaTransport->onDisplaySend = {};

    for (int chunk = 0; chunk < 3; ++chunk) {
        harness.nowNs += 50'000'000;
        harness.sendDisplayTick();
    }
    QCOMPARE(harness.mediaTransport->displays.size(), 5);

    Ps3DisplayAssembler assembler(generation);
    QList<quint64> completed;
    for (int attempt = 0; attempt < harness.mediaTransport->displays.size(); ++attempt) {
        if (attempt == 1) { continue; } // transport rejected this middle chunk
        const QByteArray& bytes = harness.mediaTransport->displays.at(attempt);
        QString error;
        const auto frame = assembler.accept(bytes, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        if (frame) { completed.append(frame->sequence); }
    }
    QCOMPARE(completed, QList<quint64>({2}));
    harness.finish();
}

// R-R3-03/R-R3-05/R-R3-09: over a link whose acknowledgements take longer
// than one send tick, SCTP takes a 49 KB PureSignal chunk only beside
// little unacknowledged data. The library then holds one chunk and the next
// is Busy. The snapshot must still complete: a held chunk counts as sent,
// a Busy chunk waits for the display channel to clear, nothing is resent,
// and a newer snapshot still replaces the one waiting to start.
void TstDaemonMediaController::ps3SnapshotCompletesWhileAcknowledgementsLag()
{
    Harness harness;
    harness.useManualDisplayTicks();
    harness.establishSession();
    harness.startReadyPeer();
    harness.mediaTransport->sctpWindowBytes = IMediaTransport::kSctpSendBufferBytes;
    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    const quint64 generation = facade->displayGeneration();
    QCOMPARE(Ps3DisplayCodec::encode(maximumPs3Snapshot(generation, 1)).size(), 3);

    facade->displaySnapshotReady(maximumPs3Snapshot(generation, 1));
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1);
    harness.sendDisplayTick(); // the second chunk does not fit: the library holds it
    QVERIFY(!harness.mediaTransport->heldDisplay.isEmpty());
    for (int tick = 0; tick < 5; ++tick) { harness.sendDisplayTick(); }
    QCOMPARE(harness.mediaTransport->displays.size(), 1);

    // Two newer snapshots arrive while the first is still going out; only
    // the newest waits to follow it.
    facade->displaySnapshotReady(maximumPs3Snapshot(generation, 2));
    facade->displaySnapshotReady(maximumPs3Snapshot(generation, 3));

    for (int round = 0; round < 20 && harness.mediaTransport->displays.size() < 6; ++round) {
        harness.mediaTransport->acknowledgeDisplayWindow();
        QCoreApplication::processEvents();
        harness.sendDisplayTick();
    }
    const QList<QByteArray>& sent = harness.mediaTransport->displays;
    QCOMPARE(sent.size(), 6);
    QCOMPARE(QSet<QByteArray>(sent.cbegin(), sent.cend()).size(), 6); // nothing resent
    Ps3DisplayAssembler assembler(generation);
    QList<quint64> completed;
    for (const QByteArray& bytes : sent) {
        QString error;
        const auto frame = assembler.accept(bytes, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        if (frame) { completed.append(frame->sequence); }
    }
    QCOMPARE(completed, QList<quint64>({1, 3}));
    // Core never offered a chunk while the library held one, and every held
    // chunk is counted as queued late rather than refused.
    QCOMPARE(harness.mediaTransport->busyDisplays, 0);
    QVERIFY(harness.mediaTransport->queuedDisplays > 0);
    const DaemonDisplayDiagnostics diagnostics = harness.controller.displayDiagnostics();
    QCOMPARE(diagnostics.displayQueuedLate, quint64(harness.mediaTransport->queuedDisplays));
    QCOMPARE(diagnostics.displaySendRefusals, quint64(0));
    harness.finish();
}

// R-R3-03/R-R3-05/R-R3-37: two spectrum endpoints and a PureSignal
// snapshot share one display channel over a link whose acknowledgements lag
// a send tick. Every acknowledgement cycle frees room for one small spectrum
// frame beside the chunk that goes out, and the next chunk is then held by
// the library. So while a snapshot is in flight spectrum gets about one
// frame per chunk cycle, whatever the tick rate, and both kinds progress:
// snapshots complete and each endpoint keeps receiving frames.
void TstDaemonMediaController::spectrumAndPs3ShareALaggingWindowAndBothProgress()
{
    Harness harness;
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    harness.mediaTransport->sctpWindowBytes = IMediaTransport::kSctpSendBufferBytes;
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    for (const quint32 endpointId : {63U, 64U}) {
        QVERIFY(harness.client.sendMediaControl(
            subscription(endpointId, 1, harness.sliceId, centre),
            harness.client.sessionEpoch()));
    }
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 63).isEmpty()
            && !messageFor(controls, QStringLiteral("context"), 64).isEmpty();
    })());

    // Each cycle's send ticks run only once Core holds a spectrum frame it
    // will send. A fixed wait for the FFT worker is outrun on a loaded
    // machine: a frame that lands after the ticks goes out a cycle late,
    // and the endpoint's 60 fps schedule then drops the next cycle's frame
    // as too close behind it. So before the ticks, in order:
    //  - more than two output periods have passed since the last ticks,
    //    so a frame produced from here on is due (every frame Core has sent
    //    was produced before those ticks);
    //  - a frame published after that is in every active source's slot;
    //  - frameAvailable has been emitted for that source since. Core takes
    //    the source's latest frame inside that emit, on this thread.
    // The I/Q is fed again on each poll until all three hold. One deadline
    // covers every cycle, so a passing run stays well inside the binary's
    // 120 s ctest TIMEOUT however the waits add up.
    auto* source = &harness.controller.sharedSpectrum()->source();
    QVERIFY(source);
    constexpr qint64 kOutputPeriodMs = 1000 / 60 + 1;
    const QDeadlineTimer cyclesDeadline(60'000);
    QElapsedTimer sinceTicks;
    sinceTicks.start();
    const auto feedDueFrame = [&](double cyclesPerSample) {
        if (!QTest::qWaitFor([&] { return sinceTicks.elapsed() > 2 * kOutputPeriodMs; },
                             cyclesDeadline)) {
            return false;
        }
        const QList<MediaSourceKey> keys = source->activeSources();
        if (keys.isEmpty()) { return false; }
        QMap<MediaSourceKey, quint64> publishedBefore;
        for (const MediaSourceKey& key : keys) {
            publishedBefore.insert(key, source->publishedFrames(key));
        }
        QSet<int> published; // indexes into keys
        QSet<int> taken;
        QSignalSpy emitted(source, &DaemonSpectrumSource::frameAvailable);
        return QTest::qWaitFor([&] {
            // An emit counts only once the newer frame was already seen:
            // one seen earlier may have handed Core an older frame.
            for (const QList<QVariant>& call : emitted) {
                const int index = keys.indexOf(call.at(0).value<MediaSourceKey>());
                if (published.contains(index)) { taken.insert(index); }
            }
            emitted.clear();
            for (int index = 0; index < keys.size(); ++index) {
                if (source->publishedFrames(keys.at(index))
                    > publishedBefore.value(keys.at(index))) {
                    published.insert(index);
                }
            }
            if (taken.size() == keys.size()) { return true; }
            harness.feedRadio(cyclesPerSample);
            return false;
        }, cyclesDeadline);
    };

    PureSignalSessionFacade* facade = harness.radio.pureSignalFacade();
    facade->setRemoteAmpViewSubscribed(true);
    const quint64 generation = facade->displayGeneration();
    quint64 sequence = 0;
    facade->displaySnapshotReady(maximumPs3Snapshot(generation, ++sequence));

    // One cycle: new spectrum for both endpoints, several send ticks (more
    // than the link can take), then the window is acknowledged.
    constexpr int kCycles = 24;
    constexpr int kTicksPerCycle = 4;
    QList<int> spectrumPerCycle;
    QList<int> ps3PerCycle;
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        harness.nowNs += 50'000'000;
        QVERIFY2(feedDueFrame(0.125 + 0.0078125 * (cycle % 16)),
                 qPrintable(QStringLiteral("no due spectrum frame in cycle %1").arg(cycle)));
        // Keep a newer snapshot waiting, so one is always in flight.
        facade->displaySnapshotReady(maximumPs3Snapshot(generation, ++sequence));
        const QList<QByteArray> before = harness.mediaTransport->displays;
        for (int tick = 0; tick < kTicksPerCycle; ++tick) { harness.sendDisplayTick(); }
        sinceTicks.restart();
        harness.mediaTransport->acknowledgeDisplayWindow();
        QCoreApplication::processEvents();
        const QList<QByteArray> sent = harness.mediaTransport->displays.mid(before.size());
        spectrumPerCycle.append(displayMessageCount(sent, QByteArrayLiteral("NSDC")));
        ps3PerCycle.append(displayMessageCount(sent, QByteArrayLiteral("PS3D")));
    }

    const QList<QByteArray>& sent = harness.mediaTransport->displays;
    QCOMPARE(QSet<QByteArray>(sent.cbegin(), sent.cend()).size(), sent.size()); // no resend
    // Spectrum: at most one frame per chunk cycle, and it keeps coming.
    const int spectrumTotal = displayMessageCount(sent, QByteArrayLiteral("NSDC"));
    QVERIFY2(*std::max_element(spectrumPerCycle.cbegin(), spectrumPerCycle.cend()) <= 1,
             qPrintable(QStringLiteral("spectrum per cycle %1").arg(
                 [&] { QStringList parts; for (int n : spectrumPerCycle) {
                           parts.append(QString::number(n)); } return parts.join(u' '); }())));
    QVERIFY2(spectrumTotal >= kCycles / 2,
             qPrintable(QStringLiteral("%1 spectrum frames in %2 cycles")
                            .arg(spectrumTotal).arg(kCycles)));
    QSet<quint32> endpoints;
    for (const QByteArray& bytes : sent) {
        if (bytes.startsWith(QByteArrayLiteral("NSDC"))) {
            endpoints.insert(qFromBigEndian<quint32>(bytes.constData() + 8));
        }
    }
    QCOMPARE(endpoints, QSet<quint32>({63U, 64U}));
    // PureSignal: a chunk in most cycles, and whole snapshots complete.
    QVERIFY(std::accumulate(ps3PerCycle.cbegin(), ps3PerCycle.cend(), 0) >= kCycles / 2);
    Ps3DisplayAssembler assembler(generation);
    int completed = 0;
    for (const QByteArray& bytes : sent) {
        if (!bytes.startsWith(QByteArrayLiteral("PS3D"))) { continue; }
        QString error;
        if (assembler.accept(bytes, &error)) { ++completed; }
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    QVERIFY2(completed >= kCycles / 6,
             qPrintable(QStringLiteral("%1 snapshots completed").arg(completed)));
    QCOMPARE(harness.mediaTransport->busyDisplays, 0);
    QCOMPARE(harness.controller.displayDiagnostics().displaySendRefusals, quint64(0));

    // The limit is the snapshot's: with PureSignal display off, the same
    // cycles carry a frame for each endpoint.
    facade->setRemoteAmpViewSubscribed(false);
    harness.mediaTransport->acknowledgeDisplayWindow();
    QCoreApplication::processEvents();
    int spectrumAlone = 0;
    constexpr int kAloneCycles = 6;
    for (int cycle = 0; cycle < kAloneCycles; ++cycle) {
        harness.nowNs += 50'000'000;
        QVERIFY2(feedDueFrame(0.25 + 0.0078125 * cycle),
                 qPrintable(QStringLiteral("no due spectrum frame alone in cycle %1")
                                .arg(cycle)));
        const qsizetype before = harness.mediaTransport->displays.size();
        for (int tick = 0; tick < kTicksPerCycle; ++tick) { harness.sendDisplayTick(); }
        sinceTicks.restart();
        harness.mediaTransport->acknowledgeDisplayWindow();
        QCoreApplication::processEvents();
        spectrumAlone += displayMessageCount(
            harness.mediaTransport->displays.mid(before), QByteArrayLiteral("NSDC"));
    }
    QVERIFY2(spectrumAlone >= 2 * kAloneCycles - 1,
             qPrintable(QStringLiteral("%1 spectrum frames alone in %2 cycles")
                            .arg(spectrumAlone).arg(kAloneCycles)));
    harness.finish();
}

void TstDaemonMediaController::radioProductionRestartWithinEpochDoesNotMintDisplayCredit()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(4096, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 26});
    harness.useManualDisplayTicks();
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QJsonObject request = subscription(
        99, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    request.insert(QStringLiteral("pixels"), 4096);
    // Include enough source bins that the first actual frame exhausts the
    // remaining credit needed for another worst-case preflight. The budget
    // charges granted pixels, so the FFT must supply all 4096 of them.
    request.insert(QStringLiteral("spanHz"), 192000.0);
    request.insert(QStringLiteral("fftSize"), 4096);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 99, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 99)
            .value(QStringLiteral("revision")).toInteger() == 1;
    })());
    harness.sendDisplayTick();
    QTRY_COMPARE(harness.mediaTransport->displays.size(), 1);

    harness.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    harness.radio.setConnectionStateForTest(ConnectionState::Connected);
    request.insert(QStringLiteral("revision"), 2);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 99, 2).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.1875);
        return messageFor(controls, QStringLiteral("context"), 99)
            .value(QStringLiteral("revision")).toInteger() == 2;
    })());
    harness.sendDisplayTick();
    QCOMPARE(harness.mediaTransport->displays.size(), 1);

    harness.nowNs = 1'000'000'000;
    QTRY_VERIFY(([&] {
        harness.feedRadio(0.21875);
        harness.sendDisplayTick();
        return harness.mediaTransport->displays.size() == 2;
    })());
    harness.finish();
}

void TstDaemonMediaController::replayedAllocationResultCanSynchronouslyRetireControllerState()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 27});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const QJsonObject request = subscription(
        100, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 100, 1).value(QStringLiteral("accepted")).toBool());
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);

    harness.stationTransport->closeOnOp = QByteArrayLiteral("allocation-result");
    const bool replaySendReturned = harness.client.sendMediaControl(
        request, harness.client.sessionEpoch());
    Q_UNUSED(replaySendReturned);
    QTRY_VERIFY(!harness.server.mediaAvailable());
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_VERIFY(harness.mediaTransport.isNull());
}

void TstDaemonMediaController::sourceRetirementPublishesZeroChargeAtLatestOperationRevision()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 28});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(101, 1, harness.sliceId, centre - 90000.0);
    request.insert(QStringLiteral("spanHz"), 1000.0);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 101, 1).value(QStringLiteral("accepted")).toBool());
    request.insert(QStringLiteral("revision"), 2);
    request.insert(QStringLiteral("pixels"), 4096);
    // The budget charges granted pixels: a crop reaching 30 kHz into the
    // source grants 160 of them, over this 128-pixel budget.
    request.insert(QStringLiteral("spanHz"), 48000.0);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 101, 2).isEmpty());
    QVERIFY(!allocationFor(controls, 101, 2).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(allocationFor(controls, 101, 2).value(QStringLiteral("acceptedRevision")).toInteger(), qint64{1});
    controls.clear();

    // Both receive slices remain inside the DDC. Only the old narrow display
    // crop loses coverage, so its retirement must be explicit to a budget GUI.
    QVERIFY(harness.radio.requestStreamCentre(harness.sliceId, centre + 50000.0));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_VERIFY(!allocationFor(controls, 101, 2).isEmpty());
    const QJsonObject retired = allocationFor(controls, 101, 2);
    QVERIFY(!retired.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(retired.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{0});
    QCOMPARE(retired.value(QStringLiteral("applicationBytesPerSecond")).toInteger(), qint64{0});
    QCOMPARE(retired.value(QStringLiteral("spectrumSampleUnitsPerSecond")).toInteger(), qint64{0});
    QVERIFY(messageFor(controls, QStringLiteral("rejected"), 101).isEmpty());

    request.insert(QStringLiteral("revision"), 3);
    request.insert(QStringLiteral("pixels"), 128);
    request.insert(QStringLiteral("spanHz"), 1000.0);
    request.insert(QStringLiteral("centreHz"), centre + 50000.0);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 101, 3).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeEndpointCount(), 1);

    QVERIFY(harness.radio.requestStreamCentre(harness.sliceId, centre - 50000.0));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_VERIFY(!allocationFor(controls, 101, 3).value(QStringLiteral("accepted")).toBool());
    controls.clear();
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 101, 3).isEmpty());
    const QJsonObject duplicateAfterRetirement = allocationFor(controls, 101, 3);
    QVERIFY(!duplicateAfterRetirement.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(duplicateAfterRetirement.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{0});
    QCOMPARE(duplicateAfterRetirement.value(QStringLiteral("applicationBytesPerSecond")).toInteger(), qint64{0});
    harness.finish();
}

void TstDaemonMediaController::failedSourceUpdateReleasesAllocationAndCanRecover()
{
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    Harness harness(DisplayBudgetLimits{cost.charge.applicationBytesPerSecond,
                                        cost.charge.spectrumSampleUnitsPerSecond, 29});
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const double centre = harness.radio.streamCentreHz(harness.streamIndex);
    QJsonObject request = subscription(102, 1, harness.sliceId, centre);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 102, 1).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeSourceCount(), 1);

    // Remove the actual producer through its existing QObject/public seam.
    // The next overlapping retune reaches update() on a now-missing source
    // and exercises its real refusal without replacing the controller logic.
    auto* source = &harness.controller.sharedSpectrum()->source();
    QVERIFY(source);
    source->deactivate({harness.streamIndex, FftTier::Wide});
    QCOMPARE(harness.controller.activeSourceCount(), 0);
    controls.clear();
    QVERIFY(harness.radio.requestStreamCentre(harness.sliceId, centre + 1000.0));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_VERIFY(!allocationFor(controls, 102, 1).isEmpty());
    const QJsonObject refused = allocationFor(controls, 102, 1);
    QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
    QCOMPARE(refused.value(QStringLiteral("acceptedRevision")).toInteger(), qint64{0});
    QCOMPARE(refused.value(QStringLiteral("applicationBytesPerSecond")).toInteger(), qint64{0});

    request.insert(QStringLiteral("revision"), 2);
    QVERIFY(harness.client.sendMediaControl(request, harness.client.sessionEpoch()));
    QTRY_VERIFY(allocationFor(controls, 102, 2).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(harness.controller.activeSourceCount(), 1);
    QTRY_VERIFY(([&] {
        harness.feedRadio();
        return !harness.mediaTransport->displays.isEmpty();
    })());
    harness.finish();
}

void TstDaemonMediaController::authenticatedControlProducesContextThenDecodedDisplayAndUnsubscribes()
{
    Harness harness;
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();

    QVERIFY(harness.client.sendMediaControl(
        subscription(7, 1, harness.sliceId,
                     harness.radio.streamCentreHz(harness.streamIndex)),
        harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);
    QTRY_COMPARE(harness.controller.activeSourceCount(), 1);

    harness.feedRadio();
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("context"), 7).isEmpty());
    const QJsonObject context = messageFor(controls, QStringLiteral("context"), 7);
    // Minor 9: today's 19 fields plus the five grant fields.
    QVERIFY(harness.server.spectrumGrantAvailable());
    QVERIFY(harness.client.spectrumGrantAvailable());
    QCOMPARE(context.size(), 24);
    const auto reported = decodeRemoteSpectrumContext(context, true);
    QVERIFY(reported.has_value() && reported->grant.has_value());
    QCOMPARE(reported->grant->grantedFftSize, 1024);
    QCOMPARE(reported->grant->grantedTier, FftTier::Wide);
    QCOMPARE(reported->grant->requestedPixels, 128);
    QCOMPARE(reported->grant->grantedPixels, 128);
    QCOMPARE(reported->grant->limit, SpectrumLimitReason::None);
    QVERIFY(!decodeRemoteSpectrumContext(context, false).has_value());
    QCOMPARE(context.value(QStringLiteral("sourceStream")).toInt(), harness.streamIndex);
    QCOMPARE(context.value(QStringLiteral("sourceCentreHz")).toDouble(),
             harness.radio.streamCentreHz(harness.streamIndex));
    QCOMPARE(context.value(QStringLiteral("sampleRateHz")).toDouble(), 192000.0);
    QCOMPARE(context.value(QStringLiteral("traceSamples")).toInt(), 128);
    QCOMPARE(context.value(QStringLiteral("waterfallSamples")).toInt(), 128);

    // The GUI requests a keyframe after it accepts context. Exercise that
    // ordering before decoding a fresh direct-media packet in isolation.
    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("keyframe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 7},
        {QStringLiteral("contextGeneration"),
         context.value(QStringLiteral("contextGeneration")).toInteger()}},
        harness.client.sessionEpoch()));
    // R-R3-21: the Core counts (and logs) each keyframe request.
    QTRY_COMPARE(harness.controller.displayDiagnostics().displayKeyframeRequests, quint64{1});
    QCOMPARE(harness.controller.displayDiagnostics().displayKeyframeRequestsRefused, quint64{0});
    QTest::qWait(20); // source cadence is 60 fps
    harness.mediaTransport->displays.clear();
    harness.feedRadio(0.1875);
    QTRY_VERIFY(!harness.mediaTransport->displays.isEmpty());

    DisplayCodecDecoder decoder;
    const DisplayCodecDecodeResult decoded = decoder.decode(
        harness.mediaTransport->displays.constLast());
    QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
    QCOMPARE(decoded.frame.context.endpointId, quint32{7});
    QCOMPARE(decoded.frame.context.contextGeneration,
             static_cast<quint32>(context.value(QStringLiteral("contextGeneration")).toInteger()));
    QCOMPARE(decoded.frame.traceDbm.size(), 128);

    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("unsubscribe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 7}}, harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_COMPARE(harness.controller.activeSourceCount(), 0);
    harness.finish();
}

void TstDaemonMediaController::fullSourceNoiseFloorFollowsContextCalibrationCadenceAndRetirement()
{
    auto& appSettings = AppSettings::instance();
    const QVariant savedMeterOffset = appSettings.value(QStringLiteral("RX1_MeterCalOffsetDb"));
    appSettings.setValue(QStringLiteral("RX1_MeterCalOffsetDb"), QStringLiteral("-3.0"));
    const auto restoreMeterOffset = qScopeGuard([&] {
        appSettings.setValue(QStringLiteral("RX1_MeterCalOffsetDb"), savedMeterOffset);
    });

    Harness harness;
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const QJsonObject initial = subscription(
        14, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    QVERIFY(harness.client.sendMediaControl(initial, harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);

    const auto hasInitialContext = [&] {
        harness.feedZeroRadio();
        return !messageFor(controls, QStringLiteral("context"), 14).isEmpty();
    };
    QTRY_VERIFY(hasInitialContext());
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("noise-floor"), 14).isEmpty());
    const QJsonObject firstFloor = messageFor(controls, QStringLiteral("noise-floor"), 14);
    QCOMPARE(firstFloor.size(), 6);
    QCOMPARE(firstFloor.value(QStringLiteral("connectionId")).toString(),
             QLatin1String(kConnectionId));
    QCOMPARE(firstFloor.value(QStringLiteral("revision")).toInteger(), qint64{1});
    const float expectedFloor = -200.0f + static_cast<float>(harness.radio.rxMeterOffsetDb());
    QVERIFY(std::abs(firstFloor.value(QStringLiteral("floorDbm")).toDouble() - expectedFloor) < 0.01);
    QVERIFY(messageIndex(controls, QStringLiteral("context"), 14)
            < messageIndex(controls, QStringLiteral("noise-floor"), 14));

    const int initialCount = messageCount(controls, QStringLiteral("noise-floor"), 14);
    harness.feedZeroRadio();
    QTest::qWait(50);
    QCOMPARE(messageCount(controls, QStringLiteral("noise-floor"), 14), initialCount);
    QTest::qWait(500);
    harness.feedZeroRadio();
    QTRY_COMPARE(messageCount(controls, QStringLiteral("noise-floor"), 14), initialCount + 1);

    // A source reconfiguration creates a new context and resets the per-
    // endpoint cadence, so its first valid frame carries a floor immediately.
    controls.clear();
    QJsonObject replacement = initial;
    replacement.insert(QStringLiteral("revision"), 2);
    replacement.insert(QStringLiteral("fftSize"), 2048);
    QVERIFY(harness.client.sendMediaControl(replacement, harness.client.sessionEpoch()));
    const auto hasReplacementContext = [&] {
        harness.feedZeroRadio(2050);
        return messageFor(controls, QStringLiteral("context"), 14)
            .value(QStringLiteral("revision")).toInteger() == 2;
    };
    QTRY_VERIFY(hasReplacementContext());
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("noise-floor"), 14).isEmpty());
    const QJsonObject replacementContext = messageFor(controls, QStringLiteral("context"), 14);
    const QJsonObject replacementFloor = messageFor(controls, QStringLiteral("noise-floor"), 14);
    QCOMPARE(replacementFloor.value(QStringLiteral("revision")).toInteger(), qint64{2});
    QCOMPARE(replacementFloor.value(QStringLiteral("contextGeneration")).toInteger(),
             replacementContext.value(QStringLiteral("contextGeneration")).toInteger());
    QVERIFY(messageIndex(controls, QStringLiteral("context"), 14)
            < messageIndex(controls, QStringLiteral("noise-floor"), 14));

    controls.clear();
    QVERIFY(harness.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("unsubscribe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 14}}, harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    harness.feedZeroRadio(2048);
    QTest::qWait(30);
    QVERIFY(messageFor(controls, QStringLiteral("noise-floor"), 14).isEmpty());
    harness.finish();
}

void TstDaemonMediaController::nonzeroNoiseFloorMatchesLocalFftBeforeCropAndQuantization()
{
    auto& settings = AppSettings::instance();
    const QVariant saved = settings.value(QStringLiteral("RX1_MeterCalOffsetDb"));
    const auto restore = qScopeGuard([&] {
        settings.setValue(QStringLiteral("RX1_MeterCalOffsetDb"), saved);
    });
    Harness harness;
    harness.establishSession();
    harness.startReadyPeer();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    for (int revision = 1; revision <= 2; ++revision) {
        const int fftSize = revision == 1 ? 1024 : 2048;
        settings.setValue(QStringLiteral("RX1_MeterCalOffsetDb"),
                          revision == 1 ? QStringLiteral("-3") : QStringLiteral("7"));
        QVector<float> iq((fftSize + 2) * 2);
        quint32 state = 0x18792345u;
        const auto noise = [&state] {
            state = state * 1664525u + 1013904223u;
            return (double(state) / double(0xffffffffu) - 0.5) * 0.002;
        };
        for (int sample = 0; sample < fftSize + 2; ++sample) {
            // Deterministic broadband input plus a strong carrier outside
            // the subscribed crop. The reference uses the complete FFT.
            const double phase = 2.0 * std::numbers::pi * 0.3 * sample;
            iq[2 * sample] = float(noise() + 0.05 * std::cos(phase));
            iq[2 * sample + 1] = float(noise() + 0.05 * std::sin(phase));
        }
        NereusSDR::FFTEngine local(0);
        local.setOutputFps(60);
        local.setFftSizeBaseline(fftSize);
        local.setFftSize(fftSize);
        local.setWindowFunction(WindowFunction::Hann);
        local.setSampleRate(192000);
        QVector<float> localBins;
        connect(&local, &NereusSDR::FFTEngine::fftReady, &local,
                [&localBins](int, const QVector<float>& bins) { localBins = bins; });
        local.feedIQ(iq);
        QCOMPARE(localBins.size(), fftSize);
        NoiseFloorEstimator estimator;
        const double expected = estimator.estimate(localBins) + harness.radio.rxMeterOffsetDb();
        QVERIFY(expected > -180); // This covers ordinary nonzero data, not the floor sentinel.

        controls.clear();
        QVERIFY(harness.client.sendMediaControl(subscription(
            15, revision, harness.sliceId,
            harness.radio.streamCentreHz(harness.streamIndex), fftSize),
            harness.client.sessionEpoch()));
        const auto hasFloor = [&] {
            QMetaObject::invokeMethod(&harness.radio, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, harness.streamIndex), Q_ARG(QVector<float>, iq));
            return messageFor(controls, QStringLiteral("noise-floor"), 15)
                .value(QStringLiteral("revision")).toInteger() == revision;
        };
        QTRY_VERIFY(hasFloor());
        const QJsonObject received = messageFor(controls, QStringLiteral("noise-floor"), 15);
        QVERIFY2(std::abs(received.value(QStringLiteral("floorDbm")).toDouble() - expected) < 0.01,
                 "Remote Clarity must match full local FFT percentile plus station calibration");
    }
    harness.finish();
}

void TstDaemonMediaController::staleRevisionAndWrongEpochPreserveTheActiveSource()
{
    Harness harness;
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    const QJsonObject accepted = subscription(
        9, 4, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex));
    QVERIFY(harness.client.sendMediaControl(accepted, harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);
    QTRY_COMPARE(harness.controller.activeSourceCount(), 1);
    harness.feedRadio();
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("context"), 9).isEmpty());
    const QJsonObject oldContext = messageFor(controls, QStringLiteral("context"), 9);

    QJsonObject stale = accepted;
    stale.insert(QStringLiteral("fftSize"), 2048);
    QVERIFY(harness.client.sendMediaControl(stale, harness.client.sessionEpoch()));
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("rejected"), 9).isEmpty());
    QCOMPARE(harness.controller.activeEndpointCount(), 1);
    QCOMPARE(harness.controller.activeSourceCount(), 1);
    QTest::qWait(20);
    harness.feedRadio(0.1875);
    QTest::qWait(20);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 9)
                 .value(QStringLiteral("contextGeneration")).toInteger(),
             oldContext.value(QStringLiteral("contextGeneration")).toInteger());

    QJsonObject newer = accepted;
    newer.insert(QStringLiteral("revision"), 5);
    QVERIFY(!harness.client.sendMediaControl(newer, harness.client.sessionEpoch() + 1));
    QCOMPARE(harness.controller.activeEndpointCount(), 1);
    QCOMPARE(harness.controller.activeSourceCount(), 1);
    harness.finish();
}

void TstDaemonMediaController::nonoverlappingCropIsRejectedBeforeSourceActivation()
{
    Harness harness;
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QJsonObject outside = subscription(
        12, 1, harness.sliceId,
        harness.radio.streamCentreHz(harness.streamIndex) + 500000.0);
    QVERIFY(harness.client.sendMediaControl(outside, harness.client.sessionEpoch()));
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("rejected"), 12).isEmpty());
    QCOMPARE(harness.controller.activeEndpointCount(), 0);
    QCOMPARE(harness.controller.activeSourceCount(), 0);
    QVERIFY(messageFor(controls, QStringLiteral("context"), 12).isEmpty());
    harness.finish();
}

void TstDaemonMediaController::streamRemovalRetiresEndpointAndSource()
{
    Harness harness;
    harness.establishSession();
    QSignalSpy controls(&harness.client, &StationClient::mediaControlReceived);
    harness.startReadyPeer();
    QVERIFY(harness.spareSliceId >= 0);
    QCOMPARE(harness.radio.slices().size(), 2);
    QVERIFY(harness.client.sendMediaControl(subscription(
        11, 1, harness.sliceId, harness.radio.streamCentreHz(harness.streamIndex)),
        harness.client.sessionEpoch()));
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 1);
    QTRY_COMPARE(harness.controller.activeSourceCount(), 1);

    harness.radio.removeSlice(harness.sliceId);
    QTRY_COMPARE(harness.controller.activeEndpointCount(), 0);
    QTRY_COMPARE(harness.controller.activeSourceCount(), 0);
    // R-R3-01/37: the retirement carries the shared wording the window's
    // refusal filter matches (SpectrumEndpoint.h), not a local copy. Removal
    // unbinds the slice's stream first, so the binding reason may win.
    QTRY_VERIFY(!messageFor(controls, QStringLiteral("rejected"), 11).isEmpty());
    const QString retired = messageFor(controls, QStringLiteral("rejected"), 11)
                                .value(QStringLiteral("reason")).toString();
    QVERIFY2(retired == QLatin1String(kRetireReasonSliceRemoved)
                 || retired == QLatin1String(kRetireReasonStreamBindingChanged),
             qPrintable(retired));
    harness.finish();
}

void TstDaemonMediaController::extendedPermissionFirstCaptureAndSharedEndpointLifetimes()
{
    Harness h;
    h.enableWidebandSource();
    h.establishSession();
    QVERIFY(h.client.remoteWidebandAvailable());
    h.startReadyPeer();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    auto request = subscription(21, 1, h.sliceId, centre);
    request.insert(QStringLiteral("extendedView"), true);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 21).isEmpty();
    })());
    auto context = messageFor(controls, QStringLiteral("context"), 21);
    QCOMPARE(context.size(), 25); // wideband and the minor-9 grant
    QVERIFY(decodeRemoteSpectrumContext(context, true).has_value());
    auto wideband = WidebandDisplayContext::fromJson(context.value(QStringLiteral("wideband")).toObject());
    QVERIFY(wideband && wideband->available && !wideband->active);
    QCOMPARE(wideband->sourceGeneration, quint32(0));
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));

    request.insert(QStringLiteral("revision"), 2);
    request.insert(QStringLiteral("spanHz"), 1'000'000.0);
    const auto packetsBefore = h.mediaTransport->displays.size();
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.p2.wbEnableMask(), quint8(1));
    // No synthetic ADC burst has been sent: capture enable alone must give
    // the first view an identity, a context and explicit empty wings.
    QVERIFY(!h.radio.latestWidebandSpectrum(0));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 21)
            .value(QStringLiteral("revision")).toInt() == 2;
    })());
    context = messageFor(controls, QStringLiteral("context"), 21);
    QCOMPARE(context.value(QStringLiteral("spanHz")).toDouble(), 1'000'000.0);
    wideband = WidebandDisplayContext::fromJson(context.value(QStringLiteral("wideband")).toObject());
    QVERIFY(wideband && wideband->active && wideband->sourceGeneration != 0);
    QCOMPARE(wideband->physicalAdcIndex, 0);
    QTRY_VERIFY(h.mediaTransport->displays.size() > packetsBefore);
    DisplayCodecDecoder decoder;
    const auto first = decoder.decode(h.mediaTransport->displays.constLast());
    QCOMPARE(first.disposition, DisplayCodecDisposition::Accepted);
    QCOMPARE(first.frame.context.contextGeneration,
             quint32(context.value(QStringLiteral("contextGeneration")).toInteger()));
    QVERIFY(std::abs(first.frame.traceDbm.first() - (-180.0f)) < 0.02f);
    QVERIFY(std::abs(first.frame.traceDbm.last() - (-180.0f)) < 0.02f);

    // Replacement rollback/invalid requests cannot disturb the active owner.
    auto malformed = request;
    malformed.insert(QStringLiteral("revision"), 3);
    malformed.insert(QStringLiteral("physicalAdcIndex"), 1);
    QVERIFY(h.client.sendMediaControl(malformed, h.client.sessionEpoch()));
    QCoreApplication::processEvents();
    QCOMPARE(h.controller.activeEndpointCount(), 1);
    QCOMPARE(h.p2.wbEnableMask(), quint8(1));

    request.insert(QStringLiteral("endpointId"), 22);
    request.insert(QStringLiteral("revision"), 1);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QCOMPARE(h.radio.widebandSourceDescriptor(0)->sourceGeneration, wideband->sourceGeneration);
    const auto unsubscribe = [&](int id) {
        return h.client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), id}}, h.client.sessionEpoch());
    };
    QVERIFY(unsubscribe(21));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QCOMPARE(h.p2.wbEnableMask(), quint8(1));
    QVERIFY(unsubscribe(22));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));
    QVERIFY(!h.radio.widebandSourceDescriptor(0));
    h.finish();
}

void TstDaemonMediaController::widebandSourceReplacementAndSessionRetirement()
{
    Harness h;
    h.enableWidebandSource();
    h.establishSession();
    h.startReadyPeer();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    auto request = subscription(31, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("extendedView"), true);
    request.insert(QStringLiteral("spanHz"), 1'000'000.0);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] { h.feedRadio(); return !messageFor(controls, QStringLiteral("context"), 31).isEmpty(); })());
    const auto before = messageFor(controls, QStringLiteral("context"), 31);
    const auto sourceBefore = h.radio.widebandSourceDescriptor(0);
    QVERIFY(sourceBefore);

    const auto accumulators = h.p2.findChildren<WidebandFrameAccumulator*>();
    QCOMPARE(accumulators.size(), 8);
    const QByteArray samples(1024, char(0x20));
    for (int sequence = 0; sequence < 32; ++sequence) {
        accumulators[0]->pushPacket(sequence, samples);
    }
    QTRY_VERIFY(h.radio.latestWidebandSpectrum(0));
    h.p2.setWidebandEnabled(0, false);
    h.p2.setWidebandEnabled(0, true);
    QVERIFY(!h.radio.latestWidebandSpectrum(0));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 31)
            .value(QStringLiteral("contextGeneration")).toInteger()
            > before.value(QStringLiteral("contextGeneration")).toInteger();
    })());
    const auto after = messageFor(controls, QStringLiteral("context"), 31);
    const auto wideband = WidebandDisplayContext::fromJson(after.value(QStringLiteral("wideband")).toObject());
    QVERIFY(wideband && wideband->active);
    QVERIFY(wideband->sourceGeneration > sourceBefore->sourceGeneration);
    QVERIFY(!h.radio.latestWidebandSpectrum(0));

    // Change only the physical ADC routing. The DDC geometry stays fixed;
    // source identity and mask must still follow the new physical input.
    DdcAssignment assignment{};
    assignment.streamDdc[h.streamIndex] = 0;
    assignment.rate[0] = 192000;
    assignment.ddcEnable = 1;
    assignment.adcCtrl1 = 1;
    h.radio.publishDdcAssignmentForTest(assignment);
    QTRY_COMPARE(h.p2.wbEnableMask(), quint8(2));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        const auto metadata = messageFor(controls, QStringLiteral("context"), 31)
            .value(QStringLiteral("wideband")).toObject();
        return metadata.value(QStringLiteral("physicalAdcIndex")).toInt(-1) == 1;
    })());
    const auto remapped = messageFor(controls, QStringLiteral("context"), 31);
    QVERIFY(remapped.value(QStringLiteral("contextGeneration")).toInteger()
            > after.value(QStringLiteral("contextGeneration")).toInteger());
    QVERIFY(!h.radio.latestWidebandSpectrum(1));
    h.finish();
    QTRY_COMPARE(h.controller.activeEndpointCount(), 0);
    QTRY_COMPARE(h.p2.wbEnableMask(), quint8(0));
}

void TstDaemonMediaController::localCaptureDuringConnectingGetsIdentityBeforeFirstAdcRow()
{
    Harness h;
    h.enableWidebandSource();
    h.radio.setConnectionStateForTest(ConnectionState::Connecting);
    h.radio.sliceById(h.sliceId)->setWidebandExtensionRequested(true);
    // The fixture has retired an earlier Connected state, so normal demand
    // is gated. Simulate P2 capture applied during the next connection setup,
    // before the model publishes Connected and re-applies retained demand.
    h.p2.setWidebandEnabled(0, true);
    QCOMPARE(h.p2.wbEnableMask(), quint8(1));
    QVERIFY(!h.radio.widebandSourceDescriptor(0));
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    // Production Connected re-publishes its DDC assignment and reconciles
    // demand. This narrow fixture runs the same reconciliation explicitly.
    h.radio.reconcileWidebandDemand();
    QVERIFY(h.radio.widebandSourceDescriptor(0));
    QVERIFY(!h.radio.latestWidebandSpectrum(0));
    h.establishSession();
    h.startReadyPeer();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    auto request = subscription(51, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("extendedView"), true);
    request.insert(QStringLiteral("spanHz"), 1'000'000.0);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 51)
            .value(QStringLiteral("wideband")).toObject().value(QStringLiteral("active")).toBool();
    })());
    QVERIFY(!h.radio.latestWidebandSpectrum(0));
    h.finish();
}

void TstDaemonMediaController::synchronousDisplayClosureRetiresDemandAndAllowsNewPeer_data()
{
    QTest::addColumn<bool>("sendAccepted");
    QTest::newRow("refused") << false;
    QTest::newRow("accepted-before-close") << true;
}

void TstDaemonMediaController::synchronousDisplayClosureRetiresDemandAndAllowsNewPeer()
{
    QFETCH(bool, sendAccepted);
    Harness h;
    h.enableWidebandSource();
    h.establishSession();
    h.startReadyPeer();
    QPointer<QObject> retiredPeer = h.mediaTransport->parent();
    bool returnedFromClose = false;
    bool peerAliveDuringSend = false;
    h.mediaTransport->onDisplaySend = [&, transport = h.mediaTransport] {
        emit transport->closed();
        peerAliveDuringSend = !retiredPeer.isNull();
        returnedFromClose = true;
        return sendAccepted;
    };
    auto request = subscription(61, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("extendedView"), true);
    request.insert(QStringLiteral("spanHz"), 1'000'000.0);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.p2.wbEnableMask(), quint8(1));
    QTRY_VERIFY(([&] { h.feedRadio(); return returnedFromClose; })());
    QVERIFY(peerAliveDuringSend);
    QCOMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.controller.activeSourceCount(), 0);
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));
    QTRY_VERIFY(retiredPeer.isNull());
    QTRY_VERIFY(h.mediaTransport.isNull());

    // The control session survives a media close. A fresh peer can reuse the
    // endpoint number without an old send modifying its new context.
    h.startReadyPeer();
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] { h.feedRadio(); return !h.mediaTransport->displays.isEmpty(); })());
    QCOMPARE(h.p2.wbEnableMask(), quint8(1));
    h.finish();
}

void TstDaemonMediaController::synchronousControlClosureRetiresDemand_data()
{
    QTest::addColumn<QByteArray>("op");
    QTest::newRow("context") << QByteArray("context");
    QTest::newRow("noise-floor") << QByteArray("noise-floor");
}

void TstDaemonMediaController::synchronousControlClosureRetiresDemand()
{
    QFETCH(QByteArray, op);
    Harness h;
    h.enableWidebandSource();
    h.establishSession();
    h.startReadyPeer();
    auto request = subscription(62, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("extendedView"), true);
    request.insert(QStringLiteral("spanHz"), 1'000'000.0);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.p2.wbEnableMask(), quint8(1));
    h.stationTransport->closeOnOp = op;
    QTRY_VERIFY(([&] { h.feedRadio(); return !h.server.mediaAvailable(); })());
    QCOMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.controller.activeSourceCount(), 0);
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));
    QTRY_VERIFY(h.mediaTransport.isNull());
}

void TstDaemonMediaController::aPeerTheCoreDropsIsToldToTheAppAtOnce_data()
{
    // The transport's failure signals, faked as the real one sends them:
    // a failed connection (ICE consent lost, or DTLS failed) reports
    // connectionFailed and then closes; a closing connection only closes.
    QTest::addColumn<QString>("failure");
    QTest::addColumn<bool>("closesWithoutFailure");
    QTest::newRow("consent-lost") << QStringLiteral("ICE consent check failed") << false;
    QTest::newRow("dtls-failed") << QStringLiteral("DTLS handshake failed") << false;
    QTest::newRow("connection-closed") << QString() << true;
}

void TstDaemonMediaController::aPeerTheCoreDropsIsToldToTheAppAtOnce()
{
    QFETCH(QString, failure);
    QFETCH(bool, closesWithoutFailure);
    Harness h;
    h.establishSession();
    h.startReadyPeer();
    auto request = subscription(71, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QPointer<FakeTransport> dropped = h.mediaTransport;
    if (!closesWithoutFailure) {
        emit dropped->connectionFailed(failure);
    }
    emit dropped->closed();

    // The app hears the whole-peer refusal for its connection, with a plain
    // reason, and nothing else about the old peer after it.
    QTRY_VERIFY(messageIndex(controls, QStringLiteral("rejected"), 0) >= 0);
    const QJsonObject rejected = messageFor(controls, QStringLiteral("rejected"), 0);
    QCOMPARE(rejected.size(), 5);
    QCOMPARE(rejected.value(QStringLiteral("connectionId")).toString(),
             QLatin1String(kConnectionId));
    QCOMPARE(rejected.value(QStringLiteral("revision")).toInteger(-1), 0);
    const QString reason = rejected.value(QStringLiteral("reason")).toString();
    QVERIFY(!reason.isEmpty());
    QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    QCOMPARE(reason, QString::fromLatin1(closesWithoutFailure ? kMediaPeerClosedReason
                                                              : kMediaPeerLostReason));
    // The library's own words stay in the Core's log.
    QVERIFY(failure.isEmpty() || !reason.contains(failure));
    QCOMPARE(messageCount(controls, QStringLiteral("rejected"), 0), 1);
    QCOMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.controller.activeSourceCount(), 0);
    QTRY_VERIFY(dropped.isNull());

    // The session carries on: the app's new start is taken at once.
    const QString fresh = QStringLiteral("22222222-3333-4444-8555-666666666666");
    QVERIFY(h.client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("start")},
                                       {QStringLiteral("connectionId"), fresh}},
                                      h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport && h.mediaTransport->started);
    QCOMPARE(messageCount(controls, QStringLiteral("rejected"), 0), 1);
    h.finish();
}

void TstDaemonMediaController::aNewStartReplacesTheSessionsHalfOpenPeer()
{
    // The phone's media died, the Core's peer has not noticed yet (it
    // still reads ready), and the device starts media again on its session.
    // A controller serves one device's session, so this is always its own
    // old peer: it is replaced, never refused.
    Harness h;
    h.establishSession();
    h.startReadyPeer();
    auto request = subscription(72, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QPointer<FakeTransport> halfOpen = h.mediaTransport;
    QVERIFY(halfOpen->readyState);
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);

    const QString fresh = QStringLiteral("22222222-3333-4444-8555-666666666666");
    QVERIFY(h.client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("start")},
                                       {QStringLiteral("connectionId"), fresh}},
                                      h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport && h.mediaTransport != halfOpen);
    QVERIFY(h.mediaTransport->started);
    // The old peer is torn down, its displays retired and its demand gone.
    QTRY_VERIFY(halfOpen.isNull() || !halfOpen->started);
    QCOMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.controller.activeSourceCount(), 0);
    QCOMPARE(h.controller.displayDemand(), DisplayBudgetCharge{});
    // Nothing refused the new start, and the old connection is not told
    // anything: the app already left it.
    QCOMPARE(messageCount(controls, QStringLiteral("rejected"), 0), 0);

    // The new peer serves the device: a display on the new connection runs.
    h.mediaTransport->becomeReady();
    request.insert(QStringLiteral("connectionId"), fresh);
    request.insert(QStringLiteral("revision"), 2);
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] { h.feedRadio(); return !h.mediaTransport->displays.isEmpty(); })());
    h.finish();
}

void TstDaemonMediaController::olderPeerKeepsLegacyContextAndCannotAcquireWideband()
{
    Harness h;
    const SpectrumDisplayCost cost = *spectrumDisplayCost(128, 60, false);
    QVERIFY(h.server.setDisplayBudgetLimits(
        {cost.charge.applicationBytesPerSecond,
         cost.charge.spectrumSampleUnitsPerSecond, 23}));
    h.enableWidebandSource();
    auto* station = new Test::LoopbackTransport(QStringLiteral("old-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("old-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteWidebandSessionProtocolMinor - 1, 6, QStringLiteral("old-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(!h.server.remoteWidebandAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    auto request = subscription(41, 1, h.sliceId, h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("extendedView"), true);
    send(request);
    QCoreApplication::processEvents();
    QCOMPARE(h.controller.activeEndpointCount(), 0);
    QCOMPARE(h.p2.wbEnableMask(), quint8(0));
    request.remove(QStringLiteral("extendedView"));
    send(request);
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QJsonObject overBudget = request;
    overBudget.insert(QStringLiteral("endpointId"), 42);
    send(overBudget);
    QTRY_VERIFY(([&] {
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::MediaControl
                && message.mediaPayload.value(QStringLiteral("op")) == QLatin1String("rejected")
                && message.mediaPayload.value(QStringLiteral("endpointId")).toInteger() == 42) {
                return true;
            }
        }
        return false;
    })());
    QCOMPARE(h.controller.activeEndpointCount(), 1);
    for (const QByteArray& wire : peer->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::MediaControl
            && message.mediaPayload.value(QStringLiteral("endpointId")).toInteger() == 42) {
            QVERIFY(message.mediaPayload.value(QStringLiteral("op"))
                    != QLatin1String("allocation-result"));
        }
    }
    QTRY_VERIFY(([&] {
        h.feedRadio();
        for (const auto& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::MediaControl
                && message.mediaPayload.value(QStringLiteral("op")) == QStringLiteral("context")) {
                return message.mediaPayload.size() == 19
                    && !message.mediaPayload.contains(QStringLiteral("wideband"));
            }
        }
        return false;
    })());
    peer->closeLink(QStringLiteral("test complete"));
}

void TstDaemonMediaController::minorSevenPeerReceivesLegacyAudioContexts()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    // A GUI that says hello with minor 7 predates the audio status detail.
    // Whatever state Core is in, it must keep receiving the exact context
    // that GUI already parses, or its audio stops.
    auto* station = new Test::LoopbackTransport(QStringLiteral("minor7-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("minor7-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteAudioStatusSessionProtocolMinor - 1, 0,
        QStringLiteral("minor-7 client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(!h.server.remoteAudioStatusAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    const qint64 ssrc = h.mediaTransport->startOptions.localAudioSsrc;
    QVERIFY(ssrc != 0);

    const auto latestContext = [&] { return receivedAudioContexts(*peer).constLast(); };
    const auto expectLegacy = [&](int count, quint32 revision, bool enabled) {
        QTRY_COMPARE(receivedAudioContexts(*peer).size(), count);
        const QJsonObject context = latestContext();
        QVERIFY2(hasLegacyAudioContextShape(context),
                 QJsonDocument(context).toJson(QJsonDocument::Compact).constData());
        QCOMPARE(context.value(QStringLiteral("connectionId")).toString(),
                 QLatin1String(kConnectionId));
        QCOMPARE(context.value(QStringLiteral("revision")).toInteger(), qint64{revision});
        QCOMPARE(context.value(QStringLiteral("enabled")).toBool(), enabled);
        QCOMPARE(context.value(QStringLiteral("ssrc")).toInteger(), ssrc);
        // What a minor-7 GUI accepts, and not what a minor-8 GUI accepts.
        QVERIFY(decodeRemoteAudioContext(context, false).has_value());
        QVERIFY(!decodeRemoteAudioContext(context, true).has_value());
    };

    // Asked for before the media peer is ready, then granted once it is.
    send(audioControl(1, true));
    expectLegacy(1, 1, false);
    if (QTest::currentTestFailed()) { return; }
    h.mediaTransport->becomeReady();
    expectLegacy(2, 1, true);
    if (QTest::currentTestFailed()) { return; }
    QCOMPARE(latestContext().value(QStringLiteral("firstSequence")).toInteger(), qint64{1});
    QCOMPARE(latestContext().value(QStringLiteral("firstTimestamp")).toInteger(), qint64{0});

    // The client turns audio off, and the station radio drops while it is off.
    send(audioControl(2, false));
    expectLegacy(3, 2, false);
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    expectLegacy(4, 2, false);
    if (QTest::currentTestFailed()) { return; }

    // Asked for while the radio is offline, granted when it returns, and
    // withdrawn again when it drops with audio still wanted.
    send(audioControl(3, true));
    expectLegacy(5, 3, false);
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    expectLegacy(6, 3, true);
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    expectLegacy(7, 3, false);
    if (QTest::currentTestFailed()) { return; }
    peer->closeLink(QStringLiteral("test complete"));
}

void TstDaemonMediaController::minorEightAudioContextsCarryEncoderOrReason()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    // The controller's sender is private; an identically built sender
    // reports the profile its encoder runs.
    const DaemonAudioSender reference(nullptr);
    const std::optional<OpusEncoderProfile> senderProfile = reference.encoderProfile();
    QVERIFY(senderProfile.has_value());
    const QJsonObject expectedEncoder = remoteAudioEncoderToJson(*senderProfile);
    QCOMPARE(expectedEncoder,
             (QJsonObject{{QStringLiteral("codec"), QStringLiteral("opus")},
                          {QStringLiteral("sampleRate"), 48000},
                          {QStringLiteral("channels"), 2},
                          {QStringLiteral("frameSamples"), 1920},
                          {QStringLiteral("targetBitrate"), 48000},
                          {QStringLiteral("audioBandwidthHz"), 20000}}));

    Harness h;
    h.establishSession();
    QVERIFY(h.client.agreedMinor() >= kRemoteAudioStatusSessionProtocolMinor);
    QVERIFY(h.server.remoteAudioStatusAvailable());
    QVERIFY(h.client.remoteAudioStatusAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
        h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);

    const auto contexts = [&controls] {
        QList<QJsonObject> found;
        for (const auto& call : controls) {
            const QJsonObject message = call.at(0).toJsonObject();
            if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
                found.append(message);
            }
        }
        return found;
    };
    const auto expectOn = [&](int count, quint32 revision) {
        QTRY_COMPARE(contexts().size(), count);
        const QJsonObject context = contexts().constLast();
        QCOMPARE(context.size(), 9);
        QVERIFY(context.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(context.value(QStringLiteral("revision")).toInteger(), qint64{revision});
        QCOMPARE(context.value(QStringLiteral("encoder")).toObject(), expectedEncoder);
        QVERIFY(!context.contains(QStringLiteral("reason")));
        const std::optional<RemoteAudioContextMessage> decoded =
            decodeRemoteAudioContext(context, true);
        QVERIFY(decoded.has_value());
        QVERIFY(decoded->encoder.has_value());
        QCOMPARE(*decoded->encoder, *senderProfile);
        QVERIFY(!decodeRemoteAudioContext(context, false).has_value());
    };
    const auto expectOff = [&](int count, quint32 revision, const char* reason) {
        QTRY_COMPARE(contexts().size(), count);
        const QJsonObject context = contexts().constLast();
        QCOMPARE(context.size(), 9);
        QVERIFY(!context.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(context.value(QStringLiteral("revision")).toInteger(), qint64{revision});
        QCOMPARE(context.value(QStringLiteral("reason")).toString(), QLatin1String(reason));
        QVERIFY(!context.contains(QStringLiteral("encoder")));
        QVERIFY(decodeRemoteAudioContext(context, true).has_value());
        QVERIFY(!decodeRemoteAudioContext(context, false).has_value());
    };
    const auto sendAudio = [&h](quint32 revision, bool enabled) {
        QVERIFY(h.client.sendMediaControl(audioControl(revision, enabled),
                                          h.client.sessionEpoch()));
    };

    // encoder-unavailable needs DaemonAudioSender::start() to fail with a
    // ready peer and a connected radio. That is unreachable here without a
    // production seam: the SSRC is never 0, RadioModel always owns an
    // AudioEngine and the default encoder always initialises.

    // Asked for before the media peer is ready, then granted once it is.
    sendAudio(1, true);
    expectOff(1, 1, "media-not-ready");
    if (QTest::currentTestFailed()) { return; }
    h.mediaTransport->becomeReady();
    expectOn(2, 1);
    if (QTest::currentTestFailed()) { return; }

    // The client's own choice outranks the radio: off, then the radio drops.
    sendAudio(2, false);
    expectOff(3, 2, "client-disabled");
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    expectOff(4, 2, "client-disabled");
    if (QTest::currentTestFailed()) { return; }

    // Asked for while the radio is offline, granted when it returns, and
    // withdrawn again when it drops with audio still wanted.
    sendAudio(3, true);
    expectOff(5, 3, "radio-offline");
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    expectOn(6, 3);
    if (QTest::currentTestFailed()) { return; }
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    expectOff(7, 3, "radio-offline");
    if (QTest::currentTestFailed()) { return; }
    h.finish();
}

// R-R3-23: nereusd's audio_bitrate reaches both the offer (the transport's
// start options, which set the SDP ceiling) and the encoder whose profile the
// minor-8 audio context reports, with the fullband sound 48 kbit/s codes.
void TstDaemonMediaController::configuredAudioBitrateReachesOfferAndContext()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    // R-R3-21: 48000 is the default; 24000 set explicitly still reaches both.
    QCOMPARE(h.controller.audioTargetBitrate(), 48000);
    h.controller.setAudioTargetBitrate(24000);
    h.establishSession();
    QVERIFY(h.server.remoteAudioStatusAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
        h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    QCOMPARE(h.mediaTransport->startOptions.role, IMediaTransport::Role::Offerer);
    QCOMPARE(h.mediaTransport->startOptions.audioTargetBitrate, 24000);

    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(audioControl(1, true), h.client.sessionEpoch()));
    const auto enabledContext = [&controls]() -> QJsonObject {
        for (const auto& call : controls) {
            const QJsonObject message = call.at(0).toJsonObject();
            if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")
                && message.value(QStringLiteral("enabled")).toBool()) {
                return message;
            }
        }
        return {};
    };
    QTRY_VERIFY(!enabledContext().isEmpty());
    const std::optional<RemoteAudioContextMessage> decoded =
        decodeRemoteAudioContext(enabledContext(), true);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->encoder.has_value());
    QCOMPARE(decoded->encoder->targetBitrate, 24000);
    QCOMPARE(decoded->encoder->audioBandwidthHz, 8000); // wideband at 24 kbit/s
    h.finish();
}

// R-R3-01/R-R3-08/R-R3-09/R-R3-37: when the pan that sized a shared engine
// leaves, every pan held to that engine is granted its own request, not
// only a lone survivor. Each renews once, for the new engine size; a pan on
// another engine is not renewed, and nothing renews when a pan that holds no
// one else back leaves. A neighbour on the grown engine that was never held
// back (it asked for no more than the engine gave) renews exactly once too:
// the engine's FFT size, and so its bin count, really changed under it. That
// is the one renewal the "no neighbour renewal" rule allows, recorded here
// as a decision (final review minor 2, 2026-09-23).
void TstDaemonMediaController::sharedEngineRegrantsEverySurvivorWhenItsSizerLeaves()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const auto contextCount = [&](quint32 endpointId, int count) {
        QTRY_VERIFY(([&] {
            h.feedRadio();
            return messageCount(controls, QStringLiteral("context"), endpointId) >= count;
        })());
    };
    const auto settle = [&] {
        for (int i = 0; i < 20; ++i) { h.feedRadio(); QTest::qWait(10); }
    };

    // E1 sizes the "wide" engine at 1024; E2 (4096) and E3 (8192) join it
    // and are held to 1024. E4 has an engine of its own. E5 joins the wide
    // engine asking for 1024, which it gets: it is not held back.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(1, 1, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    contextCount(1, 1);
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 1, h.sliceId, centre, QStringLiteral("wide"), 4096),
        h.client.sessionEpoch()));
    contextCount(2, 1);
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(3, 1, h.sliceId, centre, QStringLiteral("wide"), 8192),
        h.client.sessionEpoch()));
    contextCount(3, 1);
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(4, 1, h.sliceId, centre, QStringLiteral("fine"), 2048),
        h.client.sessionEpoch()));
    contextCount(4, 1);
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(5, 1, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    contextCount(5, 1);
    settle();
    QCOMPARE(h.controller.spectrumGrant(5)->grantedFftSize, 1024);
    QCOMPARE(h.controller.spectrumGrant(5)->reason, SpectrumLimitReason::None);
    for (quint32 held : {2U, 3U}) {
        QCOMPARE(h.controller.spectrumGrant(held)->grantedFftSize, 1024);
        QCOMPARE(h.controller.spectrumGrant(held)->reason, SpectrumLimitReason::SharedEngine);
        QCOMPARE(messageFor(controls, QStringLiteral("context"), held)
                     .value(QStringLiteral("limit")).toString(), QStringLiteral("shared"));
    }
    const int e2Contexts = messageCount(controls, QStringLiteral("context"), 2);
    const int e3Contexts = messageCount(controls, QStringLiteral("context"), 3);
    const int e4Contexts = messageCount(controls, QStringLiteral("context"), 4);
    const int e5Contexts = messageCount(controls, QStringLiteral("context"), 5);

    // E1 leaves: both pans held to its engine are re-granted. The engine
    // runs at the larger request, and neither pan is told it is shared.
    QVERIFY(h.client.sendMediaControl(unsubscription(1), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 4);
    contextCount(2, e2Contexts + 1);
    contextCount(3, e3Contexts + 1);
    contextCount(5, e5Contexts + 1);
    settle();
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 2), e2Contexts + 1);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 3), e3Contexts + 1);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 4), e4Contexts);
    // E5's one renewal: the engine grew from 1024 to 8192 bins under it.
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 5), e5Contexts + 1);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 5)
                 .value(QStringLiteral("grantedFftSize")).toInt(), 8192);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 5)
                 .value(QStringLiteral("limit")).toString(), QStringLiteral("none"));
    QCOMPARE(h.controller.spectrumGrant(5)->reason, SpectrumLimitReason::None);
    for (quint32 regranted : {2U, 3U}) {
        const QJsonObject context = messageFor(controls, QStringLiteral("context"), regranted);
        QCOMPARE(context.value(QStringLiteral("grantedFftSize")).toInt(), 8192);
        QCOMPARE(context.value(QStringLiteral("limit")).toString(), QStringLiteral("none"));
        QCOMPARE(h.controller.spectrumGrant(regranted)->grantedFftSize, 8192);
        QCOMPARE(h.controller.spectrumGrant(regranted)->reason, SpectrumLimitReason::None);
    }

    // E3 leaves. E2 is not held back by anyone, so nothing renews.
    QVERIFY(h.client.sendMediaControl(unsubscription(3), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 3);
    settle();
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 2), e2Contexts + 1);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 4), e4Contexts);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 5), e5Contexts + 1);
    QCOMPARE(h.controller.spectrumGrant(2)->reason, SpectrumLimitReason::None);
    h.finish();
}

// R-R3-01/R-R3-08: tearing the controller down with a budget session still
// live ends the pacer without a refused update. The Harness fails the test
// on that warning.
void TstDaemonMediaController::destroyingControllerWithLiveBudgetSessionIsQuiet()
{
    auto h = std::make_unique<Harness>(DisplayBudgetLimits{10'000'000, 10'000'000, 7});
    h->establishSession();
    QVERIFY(h->server.displayBudgetAvailable());
    QSignalSpy controls(&h->client, &StationClient::mediaControlReceived);
    h->startReadyPeer();
    const double centre = h->radio.streamCentreHz(h->streamIndex);
    QVERIFY(h->client.sendMediaControl(subscription(1, 1, h->sliceId, centre),
                                       h->client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 1, 1).isEmpty());
    QVERIFY(allocationFor(controls, 1, 1).value(QStringLiteral("accepted")).toBool());
    QCOMPARE(h->controller.activeEndpointCount(), 1);
    // No finish(): the controller goes while the session is live.
    h.reset();
}

namespace {

// The audio contexts a GUI has received so far, in order.
QList<QJsonObject> audioContextsIn(const QSignalSpy& controls)
{
    QList<QJsonObject> found;
    for (const auto& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
            found.append(message);
        }
    }
    return found;
}

void feedAudioBlock(Harness& h)
{
    AudioEngine* const engine = h.radio.audioEngine();
    const QVector<float> left(64 * 2, 0.20f);
    const QVector<float> right(64 * 2, 0.30f);
    for (int delivered = 0; delivered < DaemonAudioSource::kBlockFrames; delivered += 64) {
        engine->rxBlockReady(h.sliceId, left.constData(), 64);
        engine->rxBlockReady(h.spareSliceId, right.constData(), 64);
    }
}

quint16 rtpSequence(const QByteArray& packet)
{
    return qFromBigEndian<quint16>(packet.constData() + 2);
}

quint32 rtpTimestamp(const QByteArray& packet)
{
    return qFromBigEndian<quint32>(packet.constData() + 4);
}

} // namespace

// R-R3-23: a capable GUI asks for lossless. The offer carried the L16
// format, the Core switches at the next capture block (capture restarts,
// so queued Opus audio is flushed), acknowledges with the profile it runs
// and the l16 encoder shape, and sends ten L16 packets per block continuing
// the RTP clock. Asking for Opus again goes straight back; a stale revision
// is ignored.
void TstDaemonMediaController::losslessRequestSwitchesAtABlockBoundaryAndAcknowledges()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    const StationCapabilities caps = h.server.buildCapabilities();
    QCOMPARE(caps.audioProfileVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).audioProfileVersion, 1);
    QCOMPARE(h.client.capabilities().audioProfileVersion, 1);

    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    QVERIFY(h.mediaTransport->startOptions.offerLosslessAudio);
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();

    // Opus first, in the profile shape: profile "opus", no refusal.
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    QJsonObject context = audioContextsIn(controls).constLast();
    QCOMPARE(context.size(), 10);
    QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
    QVERIFY(context.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(context.value(QStringLiteral("encoder")).toObject()
                 .value(QStringLiteral("codec")).toString(), QStringLiteral("opus"));
    QVERIFY(decodeRemoteAudioContext(context, true, true).has_value());
    feedAudioBlock(h);
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 1);
    QCOMPARE(audioRtpPayloadType(h.mediaTransport->rtpPackets.constFirst()),
             OpusAudioCodecConfig::kPayloadType);

    // Lossless: acknowledged with the profile and the l16 encoder shape.
    QVERIFY(h.client.sendMediaControl(audioControl(2, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 2);
    context = audioContextsIn(controls).constLast();
    QCOMPARE(context.size(), 10);
    QCOMPARE(context.value(QStringLiteral("revision")).toInteger(), qint64{2});
    QVERIFY(context.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("lossless"));
    QCOMPARE(context.value(QStringLiteral("encoder")).toObject(),
             remoteAudioL16EncoderToJson(l16EncoderProfile()));
    QVERIFY(!context.contains(QStringLiteral("profileRefusal")));
    const std::optional<RemoteAudioContextMessage> decoded =
        decodeRemoteAudioContext(context, true, true);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->profile, std::optional{RemoteAudioProfile::Lossless});
    QCOMPARE(h.controller.audioProfile(), RemoteAudioProfile::Lossless);
    // The next block boundary: sequence and timestamp continue from the
    // Opus packet already sent.
    QCOMPARE(decoded->firstSequence, static_cast<quint16>(
        rtpSequence(h.mediaTransport->rtpPackets.constFirst()) + 1));
    QCOMPARE(decoded->firstTimestamp,
             rtpTimestamp(h.mediaTransport->rtpPackets.constFirst()) + 1920U);

    feedAudioBlock(h);
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 11);
    for (int index = 0; index < 10; ++index) {
        const QByteArray& packet = h.mediaTransport->rtpPackets.at(1 + index);
        const PcmRtpDecodeResult l16 = decodeL16Rtp(packet, decoded->ssrc);
        QCOMPARE(l16.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(l16.sequence, static_cast<quint16>(decoded->firstSequence + index));
        QCOMPARE(l16.timestamp, decoded->firstTimestamp + 192U * index);
        QVERIFY(packet.size() <= IMediaTransport::kMaxRawRtpBytes);
    }

    // A stale revision asking for Opus is ignored.
    QVERIFY(h.client.sendMediaControl(audioControl(2, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTest::qWait(50);
    QCOMPARE(audioContextsIn(controls).size(), 2);
    QCOMPARE(h.controller.audioProfile(), RemoteAudioProfile::Lossless);

    // Back to Opus at the next block, with the Opus encoder still there.
    QVERIFY(h.client.sendMediaControl(audioControl(3, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 3);
    context = audioContextsIn(controls).constLast();
    QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
    QCOMPARE(context.value(QStringLiteral("firstTimestamp")).toInteger(),
             qint64{decoded->firstTimestamp + 1920U});
    feedAudioBlock(h);
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 12);
    QCOMPARE(audioRtpPayloadType(h.mediaTransport->rtpPackets.constLast()),
             OpusAudioCodecConfig::kPayloadType);
    h.finish();
}

void TstDaemonMediaController::losslessRefusalKeepsOpusAndSaysWhy_data()
{
    QTest::addColumn<bool>("allowed");
    QTest::addColumn<bool>("declareAtStart");
    QTest::addColumn<QString>("refusal");
    QTest::newRow("core-setting-deny") << false << true << QStringLiteral("lossless-not-allowed");
    QTest::newRow("not-agreed-for-this-connection")
        << true << false << QStringLiteral("lossless-unavailable");
}

// A refusal leaves Opus running and says why, as a machine code the GUI
// turns into plain words.
void TstDaemonMediaController::losslessRefusalKeepsOpusAndSaysWhy()
{
    QFETCH(bool, allowed);
    QFETCH(bool, declareAtStart);
    QFETCH(QString, refusal);
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    h.controller.setAudioLosslessAllowed(allowed);
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(
        declareAtStart ? profileStart()
                       : QJsonObject{{QStringLiteral("op"), QStringLiteral("start")},
                                     {QStringLiteral("connectionId"),
                                      QLatin1String(kConnectionId)}},
        h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    // A Core that denies lossless never offers it; a GUI that did not
    // declare the profile gets today's offer.
    QVERIFY(!h.mediaTransport->startOptions.offerLosslessAudio);
    h.mediaTransport->becomeReady();

    g_daemonMediaMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureDaemonMediaMessages);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    const QJsonObject context = audioContextsIn(controls).constLast();
    QCOMPARE(context.size(), 11);
    QVERIFY(context.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
    QCOMPARE(context.value(QStringLiteral("profileRefusal")).toString(), refusal);
    QCOMPARE(context.value(QStringLiteral("encoder")).toObject()
                 .value(QStringLiteral("codec")).toString(), QStringLiteral("opus"));
    const std::optional<RemoteAudioContextMessage> decoded =
        decodeRemoteAudioContext(context, true, true);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->encoder.has_value());
    QCOMPARE(h.controller.audioProfile(), RemoteAudioProfile::Opus);
    // The Core's log line names the cause once.
    QCOMPARE(g_daemonMediaMessages.filter(
                 QStringLiteral("lossless audio refused (%1").arg(refusal)).size(), 1);
    h.finish();
}

// An older GUI (minor 7) has no detail shape to carry a profile: the Core
// refuses the new keys outright, and its plain start and audio control
// still get today's offer and today's eight-key context.
void TstDaemonMediaController::minorSevenPeerCannotAskForAnAudioProfile()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    auto* station = new Test::LoopbackTransport(QStringLiteral("minor7-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("minor7-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteAudioStatusSessionProtocolMinor - 1, 0,
        QStringLiteral("minor-7 client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(!h.server.remoteAudioStatusAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send(profileStart());
    QTest::qWait(50);
    QVERIFY(!h.mediaTransport);
    // R-R3-43: nor can it ask for receiver audio.
    QJsonObject receiverOnly{{QStringLiteral("op"), QStringLiteral("start")},
                             {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
                             {QStringLiteral("receiverAudioVersion"), 1}};
    send(receiverOnly);
    QTest::qWait(50);
    QVERIFY(!h.mediaTransport);
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    QVERIFY(!h.mediaTransport->startOptions.offerLosslessAudio);
    QVERIFY(h.mediaTransport->startOptions.receiverAudioSsrcs.isEmpty());
    h.mediaTransport->losslessNegotiated = true; // even so, never used
    h.mediaTransport->becomeReady();

    send(audioControl(1, true, QStringLiteral("lossless")));
    QTest::qWait(50);
    QVERIFY(receivedAudioContexts(*peer).isEmpty());
    send(audioControl(1, true));
    QTRY_COMPARE(receivedAudioContexts(*peer).size(), 1);
    QVERIFY(hasLegacyAudioContextShape(receivedAudioContexts(*peer).constLast()));
    QCOMPARE(h.controller.audioProfile(), RemoteAudioProfile::Opus);
    peer->closeLink(QStringLiteral("test complete"));
}

// Fix wave minor 7: the start's audioProfileVersion is the Core's advertised
// capability, at least 1. Zero, a fraction, a negative number or a string
// is refused and no media peer starts; version 1 is accepted.
void TstDaemonMediaController::startNeedsAnAudioProfileVersionOfAtLeastOne()
{
    Harness h;
    h.establishSession();
    QVERIFY(h.server.remoteAudioStatusAvailable());
    for (const QJsonValue& bad : {QJsonValue(0), QJsonValue(-1), QJsonValue(1.5),
                                  QJsonValue(QStringLiteral("1")), QJsonValue()}) {
        QJsonObject start = profileStart();
        start.insert(QStringLiteral("audioProfileVersion"), bad);
        QVERIFY(h.client.sendMediaControl(start, h.client.sessionEpoch()));
        QTest::qWait(20);
        QVERIFY2(!h.mediaTransport, qPrintable(QString::fromUtf8(
            QJsonDocument(QJsonObject{{QStringLiteral("v"), bad}}).toJson(QJsonDocument::Compact))));
    }
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    QVERIFY(h.mediaTransport->startOptions.offerLosslessAudio);
}

namespace {

QJsonObject clockProbe(qint64 id, qint64 t0)
{
    return {{QStringLiteral("op"), QStringLiteral("clock-probe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("id"), id},
            {QStringLiteral("t0"), t0}};
}

QList<QJsonObject> clockEchoesIn(const QSignalSpy& controls)
{
    QList<QJsonObject> found;
    for (const auto& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("clock-echo")) {
            found.append(message);
        }
    }
    return found;
}

} // namespace

// R-R3-35: the Core advertises audioClockVersion 1 with media and answers a
// clock probe with its own clock on arrival (t1) and just before the reply
// (t2), plus the newest captured audio block of the running context: its
// end as an RTP time and the capture clock's reading then, for Opus and for
// lossless. Before audio runs the capture fields are 0. Malformed probes
// get no answer.
// R-R3-21 / R-R3-08: with media on the Core advertises displayClockVersion 1,
// and a controller without an injected clock reads the producer clock that
// stamps every display frame, so a window can map display frames onto the
// audio's capture times.
void TstDaemonMediaController::displayFramesAndAudioShareTheProducerClock()
{
    Harness h;
    h.establishSession();
    const StationCapabilities caps = h.server.buildCapabilities();
    QCOMPARE(caps.displayClockVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).displayClockVersion, 1);
    QCOMPARE(h.client.capabilities().displayClockVersion, 1);

    DaemonMediaController production(&h.server, &h.radio);
    for (int i = 0; i < 3; ++i) {
        const qint64 before = DaemonSpectrumSource::monotonicNowNs();
        const qint64 media = production.mediaClockNowNs();
        const qint64 after = DaemonSpectrumSource::monotonicNowNs();
        QVERIFY2(before <= media && media <= after,
                 qPrintable(QStringLiteral("%1 <= %2 <= %3").arg(before).arg(media).arg(after)));
    }
    // The injected clock still rules a test's controller.
    h.nowNs = 42;
    QCOMPARE(h.controller.mediaClockNowNs(), qint64{42});
}

void TstDaemonMediaController::clockProbeIsAnsweredWithTheCoreClockAndCapture()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    const StationCapabilities caps = h.server.buildCapabilities();
    QCOMPARE(caps.audioClockVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).audioClockVersion, 1);
    QCOMPARE(h.client.capabilities().audioClockVersion, 1);

    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();

    // Malformed probes first, then one good one: only the good one is
    // answered, and nothing of audio yet.
    const auto epoch = h.client.sessionEpoch();
    QJsonObject extra = clockProbe(9, 1);
    extra.insert(QStringLiteral("extra"), 1);
    QJsonObject stranger = clockProbe(9, 1);
    stranger.insert(QStringLiteral("connectionId"),
                    QStringLiteral("00000000-0000-4000-8000-000000000001"));
    QJsonObject noId = clockProbe(9, 1);
    noId.remove(QStringLiteral("id"));
    for (const QJsonObject& bad : {extra, stranger, noId, clockProbe(9, -5),
                                   clockProbe(-1, 5)}) {
        QVERIFY(h.client.sendMediaControl(bad, epoch));
    }
    h.nowNs = 2'000'000'000;
    QVERIFY(h.client.sendMediaControl(clockProbe(1, 123'456'789), epoch));
    QTRY_COMPARE(clockEchoesIn(controls).size(), 1);
    QJsonObject echo = clockEchoesIn(controls).constFirst();
    QCOMPARE(sortedKeys(echo), (QStringList{
        QStringLiteral("capturedNs"), QStringLiteral("connectionId"), QStringLiteral("generation"),
        QStringLiteral("id"), QStringLiteral("op"), QStringLiteral("rtpTimestamp"),
        QStringLiteral("t0"), QStringLiteral("t1"), QStringLiteral("t2")}));
    QCOMPARE(echo.value(QStringLiteral("connectionId")).toString(), QLatin1String(kConnectionId));
    QCOMPARE(echo.value(QStringLiteral("id")).toInteger(), qint64{1});
    QCOMPARE(echo.value(QStringLiteral("t0")).toInteger(), qint64{123'456'789});
    QCOMPARE(echo.value(QStringLiteral("t1")).toInteger(), qint64{2'000'000'000});
    QCOMPARE(echo.value(QStringLiteral("t2")).toInteger(), qint64{2'000'000'000});
    QCOMPARE(echo.value(QStringLiteral("generation")).toInteger(), qint64{0});
    QCOMPARE(echo.value(QStringLiteral("rtpTimestamp")).toInteger(), qint64{0});
    QCOMPARE(echo.value(QStringLiteral("capturedNs")).toInteger(), qint64{0});

    // Opus audio: the block completes at Core time 7 s.
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")), epoch));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    const qint64 opusGeneration =
        audioContextsIn(controls).constLast().value(QStringLiteral("generation")).toInteger();
    h.nowNs = 7'000'000'000;
    feedAudioBlock(h);
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 1);
    const quint32 opusTimestamp = rtpTimestamp(h.mediaTransport->rtpPackets.constFirst());
    h.nowNs = 7'250'000'000;
    QVERIFY(h.client.sendMediaControl(clockProbe(2, 200), epoch));
    QTRY_COMPARE(clockEchoesIn(controls).size(), 2);
    echo = clockEchoesIn(controls).constLast();
    QCOMPARE(echo.value(QStringLiteral("t1")).toInteger(), qint64{7'250'000'000});
    QCOMPARE(echo.value(QStringLiteral("generation")).toInteger(), opusGeneration);
    QCOMPARE(echo.value(QStringLiteral("rtpTimestamp")).toInteger(),
             qint64{opusTimestamp + 1920U});
    QCOMPARE(echo.value(QStringLiteral("capturedNs")).toInteger(), qint64{7'000'000'000});

    // Lossless: the same, for the block its ten packets came from.
    QVERIFY(h.client.sendMediaControl(audioControl(2, true, QStringLiteral("lossless")), epoch));
    QTRY_COMPARE(audioContextsIn(controls).size(), 2);
    const qint64 losslessGeneration =
        audioContextsIn(controls).constLast().value(QStringLiteral("generation")).toInteger();
    QVERIFY(losslessGeneration != opusGeneration);
    h.nowNs = 9'000'000'000;
    feedAudioBlock(h);
    QTRY_VERIFY(h.mediaTransport->rtpPackets.size() >= 2);
    const quint32 losslessTimestamp = rtpTimestamp(h.mediaTransport->rtpPackets.at(1));
    QCOMPARE(audioRtpPayloadType(h.mediaTransport->rtpPackets.at(1)),
             PcmAudioCodecConfig::kPayloadType);
    h.nowNs = 9'100'000'000;
    QVERIFY(h.client.sendMediaControl(clockProbe(3, 300), epoch));
    QTRY_COMPARE(clockEchoesIn(controls).size(), 3);
    echo = clockEchoesIn(controls).constLast();
    QCOMPARE(echo.value(QStringLiteral("generation")).toInteger(), losslessGeneration);
    QCOMPARE(echo.value(QStringLiteral("rtpTimestamp")).toInteger(),
             qint64{losslessTimestamp + 1920U});
    QCOMPARE(echo.value(QStringLiteral("capturedNs")).toInteger(), qint64{9'000'000'000});

    // Audio off: the capture fields go back to 0.
    QVERIFY(h.client.sendMediaControl(audioControl(3, false, QStringLiteral("lossless")), epoch));
    QTRY_COMPARE(audioContextsIn(controls).size(), 3);
    QVERIFY(h.client.sendMediaControl(clockProbe(4, 400), epoch));
    QTRY_COMPARE(clockEchoesIn(controls).size(), 4);
    echo = clockEchoesIn(controls).constLast();
    QCOMPARE(echo.value(QStringLiteral("generation")).toInteger(), qint64{0});
    QCOMPARE(echo.value(QStringLiteral("capturedNs")).toInteger(), qint64{0});
    h.finish();
}


// iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1): a device that
// declared audioQuality chooses its own Opus bitrate from the measured
// table. 48000 switches the encoder and the context reports 48000 and
// 20 kHz; a bitrate not in the table is refused with its reason and the
// running one stays; 24000 (the phone's "save data" choice) reports 8 kHz.
void TstDaemonMediaController::aDeviceChoosesItsOwnMeasuredOpusBitrate()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    h.controller.setAudioTargetBitrate(24000);
    h.client.declareFeatureForTest(QByteArrayLiteral("audioQuality"), 1);
    h.establishSession();
    QCOMPARE(h.client.capabilities().audioQualityVersion, 1);
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    QJsonObject context;
    const auto ask = [&](quint32 revision, int bitrate) {
        QJsonObject control = audioControl(revision, true, QStringLiteral("opus"));
        control.insert(QStringLiteral("opusBitrate"), bitrate);
        const qsizetype before = audioContextsIn(controls).size();
        QVERIFY(h.client.sendMediaControl(control, h.client.sessionEpoch()));
        QTRY_VERIFY(audioContextsIn(controls).size() > before);
        context = audioContextsIn(controls).constLast();
    };

    ask(1, 48000);
    std::optional<RemoteAudioContextMessage> decoded =
        decodeRemoteAudioContext(context, true, true);
    QVERIFY2(decoded.has_value(), QJsonDocument(context).toJson().constData());
    QVERIFY(decoded->enabled && decoded->encoder.has_value());
    QCOMPARE(decoded->encoder->targetBitrate, 48000);
    QCOMPARE(decoded->encoder->audioBandwidthHz, 20000);
    QVERIFY(!context.contains(QStringLiteral("opusBitrateRefusal")));
    QCOMPARE(h.controller.audioStreamBitrate(), 48000);

    // Not in the table: refused with its reason; the running one stays.
    ask(2, 32000);
    decoded = decodeRemoteAudioContext(context, true, true);
    QVERIFY2(decoded.has_value(), QJsonDocument(context).toJson().constData());
    QCOMPARE(decoded->opusBitrateRefusal, opusBitrateNotOfferedReason());
    QVERIFY(OperatorWording::isPlain(decoded->opusBitrateRefusal));
    QVERIFY(decoded->encoder.has_value());
    QCOMPARE(decoded->encoder->targetBitrate, 48000);
    QCOMPARE(h.controller.audioStreamBitrate(), 48000);

    // The save-data choice.
    ask(3, 24000);
    decoded = decodeRemoteAudioContext(context, true, true);
    QVERIFY(decoded.has_value() && decoded->encoder.has_value());
    QCOMPARE(decoded->encoder->targetBitrate, 24000);
    QCOMPARE(decoded->encoder->audioBandwidthHz, 8000);
    QVERIFY(decoded->opusBitrateRefusal.isEmpty());
    h.finish();
}

// ...and a device that never sends opusBitrate (every app before it) gets
// the Core's audio_bitrate exactly as before: no audioQualityVersion, the
// context's shape unchanged, and an opusBitrate from it is not read.
void TstDaemonMediaController::aDeviceThatNeverAsksGetsTheCoresBitrate()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    h.controller.setAudioTargetBitrate(24000);
    h.establishSession();
    QCOMPARE(h.client.capabilities().audioQualityVersion, 0);
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    const QJsonObject context = audioContextsIn(controls).constLast();
    const QStringList keys = context.keys();
    QCOMPARE(keys, (QStringList{QStringLiteral("connectionId"), QStringLiteral("enabled"),
                                QStringLiteral("encoder"), QStringLiteral("firstSequence"),
                                QStringLiteral("firstTimestamp"), QStringLiteral("generation"),
                                QStringLiteral("op"), QStringLiteral("profile"),
                                QStringLiteral("revision"), QStringLiteral("ssrc")}));
    QCOMPARE(context.value(QStringLiteral("encoder")).toObject()
                 .value(QStringLiteral("targetBitrate")).toInt(), 24000);

    // Its opusBitrate is not one it may send: the control is not read.
    QJsonObject control = audioControl(2, true, QStringLiteral("opus"));
    control.insert(QStringLiteral("opusBitrate"), 48000);
    QVERIFY(h.client.sendMediaControl(control, h.client.sessionEpoch()));
    QTest::qWait(100);
    QCOMPARE(audioContextsIn(controls).size(), 1);
    QCOMPARE(h.controller.audioStreamBitrate(), 24000);
    h.finish();
}

namespace {

// R-R3-43: the start of a GUI that understands audio profiles and asks for
// receiver audio.
QJsonObject receiverStart()
{
    QJsonObject start = profileStart();
    start.insert(QStringLiteral("receiverAudioVersion"), 1);
    return start;
}

QJsonObject receiverAudioControl(int sliceId, quint32 revision, bool enabled,
                                 const QString& profile = QStringLiteral("opus"))
{
    return {{QStringLiteral("op"), QStringLiteral("receiver-audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("profile"), profile}};
}

QList<QJsonObject> receiverContextsIn(const QSignalSpy& controls, int sliceId = -1)
{
    QList<QJsonObject> found;
    for (const auto& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("receiver-audio-context")
            && (sliceId < 0 || message.value(QStringLiteral("sliceId")).toInt() == sliceId)) {
            found.append(message);
        }
    }
    return found;
}

quint32 rtpSsrcOf(const QByteArray& packet)
{
    return qFromBigEndian<quint32>(packet.constData() + 8);
}

QList<QByteArray> packetsWithSsrc(const QList<QByteArray>& packets, quint32 ssrc)
{
    QList<QByteArray> found;
    for (const QByteArray& packet : packets) {
        if (packet.size() >= 12 && rtpSsrcOf(packet) == ssrc) {
            found.append(packet);
        }
    }
    return found;
}

// Feeds one 1920-frame block to every slice in `slices`, each its own level.
void feedSlicesBlock(Harness& h, const QList<QPair<int, float>>& slices)
{
    AudioEngine* const engine = h.radio.audioEngine();
    for (int delivered = 0; delivered < DaemonAudioSource::kBlockFrames; delivered += 64) {
        for (const auto& [sliceId, level] : slices) {
            const QVector<float> block(64 * 2, level);
            engine->rxBlockReady(sliceId, block.constData(), 64);
        }
    }
}

} // namespace

// R-R3-43 (Task 1's finding): libdatachannel hands an app every packet on
// the one audio line, declared or not, and an app that did not ask refuses
// each one and reports it. So an app that did not declare receiverAudioVersion
// at start gets no receiver stream whatever it asks: no stream ids in the
// offer, no receiver context, no packet on anything but the main id.
void TstDaemonMediaController::olderAppNeverReceivesReceiverStreams()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    // The Core advertises it with media, and a capable GUI would see it.
    QCOMPARE(h.server.buildCapabilities().receiverAudioVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(
                 h.server.buildCapabilities().toUpdates()).receiverAudioVersion, 1);
    QCOMPARE(h.client.capabilities().receiverAudioVersion, 1);

    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    // A start with the capability version malformed starts nothing.
    for (const QJsonValue& bad : {QJsonValue(0), QJsonValue(1.5), QJsonValue(-1),
                                  QJsonValue(QStringLiteral("1")), QJsonValue()}) {
        QJsonObject start = receiverStart();
        start.insert(QStringLiteral("receiverAudioVersion"), bad);
        QVERIFY(h.client.sendMediaControl(start, h.client.sessionEpoch()));
        QTest::qWait(20);
        QVERIFY(!h.mediaTransport);
    }
    // Today's start: today's offer, with no receiver stream ids.
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    QVERIFY(h.mediaTransport->startOptions.receiverAudioSsrcs.isEmpty());
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();
    const quint32 mainSsrc = h.mediaTransport->startOptions.localAudioSsrc;

    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    quint32 revision = 1;
    for (int sliceId : {h.sliceId, h.spareSliceId}) {
        for (const QString& profile : {QStringLiteral("opus"), QStringLiteral("lossless")}) {
            QVERIFY(h.client.sendMediaControl(
                receiverAudioControl(sliceId, revision++, true, profile),
                h.client.sessionEpoch()));
        }
    }
    for (int block = 0; block < 3; ++block) {
        feedSlicesBlock(h, {{h.sliceId, 0.2f}, {h.spareSliceId, 0.3f}});
    }
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 3);
    QTest::qWait(50);
    QVERIFY(receiverContextsIn(controls).isEmpty());
    QCOMPARE(audioContextsIn(controls).size(), 1);
    QCOMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(),
             h.mediaTransport->rtpPackets.size());
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 0);
    QCOMPARE(engine->sliceAudioTapCount(), 0);
    h.finish();
}

// A receiver stream starts, stops and restarts on its own stream id while
// the speakers' stream runs untouched: no new main context, no gap in the
// main stream's sequence. Turning the main stream off leaves it running.
// Lossless, so the packets decode to exactly slice B's level.
void TstDaemonMediaController::receiverStreamRunsBesideTheMainWithoutRestartingIt()
{
    Harness h;
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(receiverStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    const QList<quint32> receiverSsrcs = MediaPeer::receiverAudioSsrcsForConnection(
        QLatin1String(kConnectionId));
    QCOMPARE(h.mediaTransport->startOptions.receiverAudioSsrcs, receiverSsrcs);
    const quint32 mainSsrc = h.mediaTransport->startOptions.localAudioSsrc;
    h.mediaTransport->losslessNegotiated = true;
    // Asked for before the connection is ready: off, media-not-ready, and
    // the stream id is already its own.
    const int sliceB = h.spareSliceId;
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(sliceB, 1, true,
                                                           QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls).size(), 1);
    QJsonObject context = receiverContextsIn(controls).constLast();
    QCOMPARE(context.value(QStringLiteral("reason")).toString(),
             QStringLiteral("media-not-ready"));
    QCOMPARE(static_cast<quint32>(context.value(QStringLiteral("ssrc")).toInteger()),
             receiverSsrcs.at(0));

    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);
    QTRY_COMPARE(receiverContextsIn(controls).size(), 2);
    context = receiverContextsIn(controls).constLast();
    std::optional<RemoteReceiverAudioContextMessage> decoded =
        decodeReceiverAudioContext(context);
    QVERIFY2(decoded.has_value(), QJsonDocument(context).toJson().constData());
    QCOMPARE(decoded->sliceId, sliceB);
    QVERIFY(decoded->context.enabled);
    QCOMPARE(decoded->context.ssrc, receiverSsrcs.at(0));
    QCOMPARE(decoded->context.firstSequence, quint16{1});
    QCOMPARE(decoded->context.firstTimestamp, quint32{0});
    QCOMPARE(decoded->context.profile, std::optional{RemoteAudioProfile::Lossless});
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 1);
    QCOMPARE(engine->sliceAudioTapCount(), 1);

    feedSlicesBlock(h, {{h.sliceId, 0.25f}, {sliceB, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrcs.at(0)).size(), 10);
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 10);
    for (const QByteArray& packet :
         packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrcs.at(0))) {
        const PcmRtpDecodeResult l16 = decodeL16Rtp(packet, receiverSsrcs.at(0));
        QCOMPARE(l16.status, OpusAudioCodecStatus::Accepted);
        for (float sample : l16.pcmInterleaved) {
            QVERIFY(std::abs(sample - 0.5f) < 1.0e-4f);
        }
    }

    // Stop, then restart: two receiver contexts, the stream id's timeline
    // continuing, and nothing at all for the main stream.
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(sliceB, 2, false,
                                                           QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls).size(), 3);
    context = receiverContextsIn(controls).constLast();
    QCOMPARE(context.value(QStringLiteral("reason")).toString(),
             QStringLiteral("client-disabled"));
    QCOMPARE(engine->sliceAudioTapCount(), 0);
    feedSlicesBlock(h, {{h.sliceId, 0.25f}, {sliceB, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 20);
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(sliceB, 3, true,
                                                           QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls).size(), 4);
    decoded = decodeReceiverAudioContext(receiverContextsIn(controls).constLast());
    QVERIFY(decoded.has_value() && decoded->context.enabled);
    QCOMPARE(decoded->context.ssrc, receiverSsrcs.at(0));
    QCOMPARE(decoded->context.firstSequence, quint16{11});
    QCOMPARE(decoded->context.firstTimestamp, quint32{1920});

    // The main stream turns off; the receiver stream keeps flowing.
    QVERIFY(h.client.sendMediaControl(audioControl(2, false, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 2);
    feedSlicesBlock(h, {{h.sliceId, 0.25f}, {sliceB, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrcs.at(0)).size(), 20);
    QCOMPARE(receiverContextsIn(controls).size(), 4);

    // One main context per main control, none from the receiver churn, and
    // the main stream's sequence has no gap.
    QCOMPARE(audioContextsIn(controls).size(), 2);
    const QList<QByteArray> mainPackets =
        packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc);
    for (int index = 1; index < mainPackets.size(); ++index) {
        QCOMPARE(rtpSequence(mainPackets.at(index)),
                 static_cast<quint16>(rtpSequence(mainPackets.at(index - 1)) + 1));
        QCOMPARE(rtpTimestamp(mainPackets.at(index)),
                 rtpTimestamp(mainPackets.at(index - 1)) + 192U);
    }
    h.finish();
    QTRY_COMPARE(h.controller.activeReceiverAudioStreamCount(), 0);
    QCOMPARE(engine->sliceAudioTapCount(), 0);
}

// A revision at or below the slice's last is ignored; four streams run at
// once and a fifth is refused with receiver-limit and no stream id. The
// refused one starts when asked again after another stream has gone.
void TstDaemonMediaController::staleReceiverRevisionIsIgnoredAndAFifthStreamIsRefused()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    QList<int> slices{h.sliceId, h.spareSliceId};
    for (int added = 0; added < 3; ++added) {
        const int sliceId = h.radio.addSlice();
        QVERIFY(sliceId >= 0);
        slices.append(sliceId);
    }
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(receiverStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    const QList<quint32> receiverSsrcs = h.mediaTransport->startOptions.receiverAudioSsrcs;
    QCOMPARE(receiverSsrcs.size(), 4);

    for (int index = 0; index < 4; ++index) {
        QVERIFY(h.client.sendMediaControl(receiverAudioControl(slices.at(index), 5, true),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(receiverContextsIn(controls, slices.at(index)).size(), 1);
        const QJsonObject context = receiverContextsIn(controls, slices.at(index)).constLast();
        QVERIFY(context.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(static_cast<quint32>(context.value(QStringLiteral("ssrc")).toInteger()),
                 receiverSsrcs.at(index));
    }
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 4);

    const int fifth = slices.at(4);
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(fifth, 1, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls, fifth).size(), 1);
    QJsonObject refused = receiverContextsIn(controls, fifth).constLast();
    QVERIFY(!refused.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
             QStringLiteral("receiver-limit"));
    QCOMPARE(refused.value(QStringLiteral("ssrc")).toInteger(), qint64{0});
    QVERIFY(decodeReceiverAudioContext(refused).has_value());
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 4);

    // Stale or equal revisions: nothing happens, nothing is answered.
    const int contextsBefore = receiverContextsIn(controls).size();
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(slices.at(0), 5, false),
                                      h.client.sessionEpoch()));
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(slices.at(0), 4, false),
                                      h.client.sessionEpoch()));
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(fifth, 1, true),
                                      h.client.sessionEpoch()));
    // Malformed requests are ignored too.
    QJsonObject extra = receiverAudioControl(slices.at(1), 9, false);
    extra.insert(QStringLiteral("extra"), 1);
    QVERIFY(h.client.sendMediaControl(extra, h.client.sessionEpoch()));
    QVERIFY(h.client.sendMediaControl(
        receiverAudioControl(slices.at(1), 9, false, QStringLiteral("flac")),
        h.client.sessionEpoch()));
    QTest::qWait(50);
    QCOMPARE(receiverContextsIn(controls).size(), contextsBefore);
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 4);

    // A slice that does not exist is answered slice-removed and kept nowhere.
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(4242, 1, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls, 4242).size(), 1);
    QCOMPARE(receiverContextsIn(controls, 4242).constLast()
                 .value(QStringLiteral("reason")).toString(),
             QStringLiteral("slice-removed"));

    // One stream lets go; the fifth asks again and takes its stream id.
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(slices.at(2), 6, false),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls, slices.at(2)).size(), 2);
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(fifth, 2, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls, fifth).size(), 2);
    const QJsonObject granted = receiverContextsIn(controls, fifth).constLast();
    QVERIFY(granted.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(static_cast<quint32>(granted.value(QStringLiteral("ssrc")).toInteger()),
             receiverSsrcs.at(2));
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 4);
    QCOMPARE(h.radio.audioEngine()->sliceAudioTapCount(), 4);
    // No main context was ever sent: none was asked for.
    QVERIFY(audioContextsIn(controls).isEmpty());
    h.finish();
}

// A receiver stream runs the session's one quality choice: Opus, or
// lossless when asked and the rules admit it; refused lossless keeps Opus
// and says why, as the main stream does. Compressed, it runs Opus at 48
// kbit/s fullband (kReceiverAudioOpusBitrate) whatever audio_bitrate says:
// when Opus is the choice, when lossless is refused, and when lossless
// falls back (the window asks for Opus again). The speakers' mix keeps the
// Core's audio_bitrate, and every packet the receiver stream sends is coded
// fullband.
void TstDaemonMediaController::receiverStreamFollowsTheSessionAudioProfile()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    QCOMPARE(DaemonMediaController::kReceiverAudioOpusBitrate, 48'000);
    OpusAudioCodecConfig receiverConfig;
    receiverConfig.bitrate = DaemonMediaController::kReceiverAudioOpusBitrate;
    const DaemonAudioSender receiverReference(nullptr, receiverConfig);
    QVERIFY(receiverReference.encoderProfile().has_value());
    QCOMPARE(receiverReference.encoderProfile()->targetBitrate, 48'000);
    QCOMPARE(receiverReference.encoderProfile()->audioBandwidthHz, 20'000);
    const QJsonObject expectedReceiver =
        remoteAudioEncoderToJson(*receiverReference.encoderProfile());

    // The speakers' mix at each audio_bitrate the Core accepts, and the
    // media connection with and without the lossless format.
    for (const int mainBitrate : {24'000, 48'000}) {
        OpusAudioCodecConfig mainConfig;
        mainConfig.bitrate = mainBitrate;
        const DaemonAudioSender mainReference(nullptr, mainConfig);
        QVERIFY(mainReference.encoderProfile().has_value());
        const QJsonObject expectedMain = remoteAudioEncoderToJson(*mainReference.encoderProfile());
        for (const bool losslessNegotiated : {true, false}) {
            const QByteArray row = QByteArray::number(mainBitrate)
                + (losslessNegotiated ? " lossless offered" : " no lossless");
            Harness h;
            AudioEngine* const engine = h.radio.audioEngine();
            engine->setSliceStreaming(h.spareSliceId, true);
            h.controller.setAudioTargetBitrate(mainBitrate);
            h.establishSession();
            QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
            QVERIFY(h.client.sendMediaControl(receiverStart(), h.client.sessionEpoch()));
            QTRY_VERIFY(h.mediaTransport);
            // The offer is today's: the speakers' rate, not the receivers'.
            QCOMPARE(h.mediaTransport->startOptions.audioTargetBitrate, mainBitrate);
            const quint32 receiverSsrc = h.mediaTransport->startOptions.receiverAudioSsrcs.at(0);
            h.mediaTransport->losslessNegotiated = losslessNegotiated;
            h.mediaTransport->becomeReady();

            QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("opus")),
                                              h.client.sessionEpoch()));
            QTRY_COMPARE(audioContextsIn(controls).size(), 1);
            QCOMPARE(audioContextsIn(controls).constLast().value(QStringLiteral("encoder"))
                         .toObject(), expectedMain);

            // Opus chosen.
            QVERIFY(h.client.sendMediaControl(receiverAudioControl(h.spareSliceId, 1, true),
                                              h.client.sessionEpoch()));
            QTRY_COMPARE(receiverContextsIn(controls).size(), 1);
            QJsonObject context = receiverContextsIn(controls).constLast();
            QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
            QVERIFY2(context.value(QStringLiteral("encoder")).toObject() == expectedReceiver,
                     row.constData());
            QVERIFY(!context.contains(QStringLiteral("profileRefusal")));
            QCOMPARE(h.controller.receiverAudioProfile(h.spareSliceId),
                     std::optional{RemoteAudioProfile::Opus});
            feedSlicesBlock(h, {{h.spareSliceId, 0.5f}});
            QTRY_VERIFY(!packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrc).isEmpty());

            // Lossless asked for: lossless when offered, else Opus at 48
            // kbit/s with the refusal.
            QVERIFY(h.client.sendMediaControl(
                receiverAudioControl(h.spareSliceId, 2, true, QStringLiteral("lossless")),
                h.client.sessionEpoch()));
            QTRY_COMPARE(receiverContextsIn(controls).size(), 2);
            context = receiverContextsIn(controls).constLast();
            QVERIFY(context.value(QStringLiteral("enabled")).toBool());
            if (losslessNegotiated) {
                QCOMPARE(context.value(QStringLiteral("profile")).toString(),
                         QStringLiteral("lossless"));
                QCOMPARE(context.value(QStringLiteral("encoder")).toObject(),
                         remoteAudioL16EncoderToJson(l16EncoderProfile()));
                QCOMPARE(h.controller.receiverAudioProfile(h.spareSliceId),
                         std::optional{RemoteAudioProfile::Lossless});
            } else {
                QCOMPARE(context.value(QStringLiteral("profile")).toString(),
                         QStringLiteral("opus"));
                QCOMPARE(context.value(QStringLiteral("profileRefusal")).toString(),
                         QStringLiteral("lossless-unavailable"));
                QVERIFY2(context.value(QStringLiteral("encoder")).toObject() == expectedReceiver,
                         row.constData());
            }
            QVERIFY(decodeReceiverAudioContext(context).has_value());

            // Lossless falls back: the window asks for Opus again after its
            // link trial. Still 48 kbit/s, not the speakers' rate.
            QVERIFY(h.client.sendMediaControl(
                receiverAudioControl(h.spareSliceId, 3, true, QStringLiteral("opus")),
                h.client.sessionEpoch()));
            QTRY_COMPARE(receiverContextsIn(controls).size(), 3);
            context = receiverContextsIn(controls).constLast();
            QCOMPARE(context.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
            QVERIFY2(context.value(QStringLiteral("encoder")).toObject() == expectedReceiver,
                     row.constData());
            const std::optional<RemoteReceiverAudioContextMessage> decoded =
                decodeReceiverAudioContext(context);
            QVERIFY(decoded.has_value() && decoded->context.encoder.has_value());
            QCOMPARE(decoded->context.encoder->targetBitrate, 48'000);
            const qsizetype before = packetsWithSsrc(h.mediaTransport->rtpPackets,
                                                     receiverSsrc).size();
            feedSlicesBlock(h, {{h.spareSliceId, 0.5f}});
            QTRY_VERIFY(packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrc).size()
                        > before);

            // Every Opus packet on the receiver stream is coded fullband.
            int opusPackets = 0;
            for (const QByteArray& packet :
                 packetsWithSsrc(h.mediaTransport->rtpPackets, receiverSsrc)) {
                if (audioRtpPayloadType(packet) != OpusAudioCodecConfig::kPayloadType) {
                    continue;
                }
                const OpusRtpInspection inspected = inspectOpusRtp(packet, receiverSsrc);
                QCOMPARE(inspected.status, OpusAudioCodecStatus::Accepted);
                QCOMPARE(inspected.packetInfo.bandwidth, bandwidthForBitrate(48'000));
                ++opusPackets;
            }
            QVERIFY(opusPackets >= 2);

            // The main stream was not touched by any of it, and still runs
            // at the Core's audio_bitrate.
            QCOMPARE(audioContextsIn(controls).size(), 1);
            QCOMPARE(h.controller.audioTargetBitrate(), mainBitrate);
            engine->setSliceStreaming(h.spareSliceId, false);
            h.finish();
        }
    }

    // The Core's own setting refuses lossless for receiver streams too.
    Harness h;
    h.controller.setAudioLosslessAllowed(false);
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(receiverStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(
        receiverAudioControl(h.sliceId, 1, true, QStringLiteral("lossless")),
        h.client.sessionEpoch()));
    QTRY_COMPARE(receiverContextsIn(controls).size(), 1);
    QCOMPARE(receiverContextsIn(controls).constLast()
                 .value(QStringLiteral("profileRefusal")).toString(),
             QStringLiteral("lossless-not-allowed"));
    h.finish();
}

// Removing the slice, the radio going offline and the session ending each
// retire the stream; the first two say why, and nothing is left behind:
// no sender running and no slice tap in the audio engine.
void TstDaemonMediaController::receiverStreamRetiresOnSliceRadioAndSessionEnd()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    AudioEngine* const engine = h.radio.audioEngine();
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(receiverStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    const QList<quint32> receiverSsrcs = h.mediaTransport->startOptions.receiverAudioSsrcs;

    QVERIFY(h.client.sendMediaControl(receiverAudioControl(h.sliceId, 1, true),
                                      h.client.sessionEpoch()));
    QVERIFY(h.client.sendMediaControl(receiverAudioControl(h.spareSliceId, 1, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeReceiverAudioStreamCount(), 2);
    QCOMPARE(engine->sliceAudioTapCount(), 2);

    // The radio goes: both stop with radio-offline and keep their ids.
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    QTRY_COMPARE(receiverContextsIn(controls).size(), 4);
    for (int sliceId : {h.sliceId, h.spareSliceId}) {
        const QJsonObject off = receiverContextsIn(controls, sliceId).constLast();
        QCOMPARE(off.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("radio-offline"));
        QVERIFY(off.value(QStringLiteral("ssrc")).toInteger() != 0);
    }
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 0);
    QCOMPARE(engine->sliceAudioTapCount(), 0);
    // And both come back on the same ids when it returns.
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    QTRY_COMPARE(receiverContextsIn(controls).size(), 6);
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 2);
    QCOMPARE(static_cast<quint32>(receiverContextsIn(controls, h.sliceId).constLast()
                                      .value(QStringLiteral("ssrc")).toInteger()),
             receiverSsrcs.at(0));

    // The slice goes: its stream stops with slice-removed; the other runs.
    h.radio.removeSlice(h.sliceId);
    QTRY_COMPARE(receiverContextsIn(controls, h.sliceId).size(), 4);
    const QJsonObject removed = receiverContextsIn(controls, h.sliceId).constLast();
    QCOMPARE(removed.value(QStringLiteral("reason")).toString(),
             QStringLiteral("slice-removed"));
    QVERIFY(decodeReceiverAudioContext(removed).has_value());
    QCOMPARE(h.controller.activeReceiverAudioStreamCount(), 1);
    QCOMPARE(engine->sliceAudioTapCount(), 1);
    QCOMPARE(h.controller.receiverAudioProfile(h.sliceId), std::nullopt);

    // The session ends: the last stream and its tap go with it.
    h.finish();
    QTRY_COMPARE(h.controller.activeReceiverAudioStreamCount(), 0);
    QCOMPARE(engine->sliceAudioTapCount(), 0);
    QTRY_COMPARE(h.controller.findChildren<DaemonAudioSender*>().size(), 0);
}

// R-IOS-13, R-R3-42 (2026-09-27): the line logged at each unkey carries the
// microphone path's added latency (mean, max), the send ring's fill (mean,
// max) and the silence shed, besides the counters it always had.
void TstDaemonMediaController::unkeyLineCarriesTheMicrophonePathsLatency()
{
    RemoteMicReceiver::Stats rx;
    rx.concealedPackets = 2;
    RemoteMicFeed::Stats feed;
    feed.fillFrames = 960;
    feed.targetFrames = 1440;
    feed.addedMeanMs = 22.46;
    feed.addedMaxMs = 222.8;
    feed.ringMeanMs = 2.42;
    feed.ringMaxMs = 200.0;
    feed.shedFrames = 384;
    feed.shedForRingFrames = 9024;
    feed.insertedFrames = 0;
    feed.grows = 1;
    feed.heldBlocks = 42;
    // TX stall lane: the over's waits at the Core's event loop.
    feed.ownerWaitMeanMs = 4.25;
    feed.ownerWaitMaxMs = 91.0;
    feed.ownerWaitsLong = 5;
    RadioConnection::TxSendStats send;
    send.valid = true;
    send.framesSent = 9600;
    const QString line =
        DaemonMediaController::unkeyStatsLine("phone-1", rx, &feed, send, 91'456);
    QVERIFY2(line.contains(QStringLiteral("target 30 ms")), qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral("added latency mean 22.5 ms, max 222.8 ms")),
             qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral("send ring mean 2.4 ms, max 200.0 ms")),
             qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral("shed 196.0 ms (188.0 ms for the ring), inserted 0.0 ms")),
             qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral("target grew 1 times, held for DEXP 42 blocks")),
             qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral(
                 "held for DEXP 42 blocks; line waits mean 4.3 ms, max 91.0 ms, 5 over 50 ms; "
                 "keepalive waits max 91.5 ms; packets concealed 2")),
             qPrintable(line));
    // TX mic thread fix round 2: an over with no "tx" keepalive says so.
    QVERIFY(DaemonMediaController::unkeyStatsLine("phone-1", rx, &feed, send)
                .contains(QStringLiteral("; keepalive waits none; ")));
    QVERIFY2(line.contains(QStringLiteral("packets concealed 2")), qPrintable(line));
    QVERIFY2(line.contains(QStringLiteral("frames 9600")), qPrintable(line));

    // A path whose connection does not report its ring says so.
    feed.ringMeanMs = -1.0;
    feed.ringMaxMs = -1.0;
    QVERIFY(DaemonMediaController::unkeyStatsLine("phone-1", rx, &feed, send)
                .contains(QStringLiteral("send ring unknown")));
    QVERIFY(DaemonMediaController::unkeyStatsLine("phone-1", rx, nullptr, send)
                .contains(QStringLiteral("microphone no feed")));

    // G-07: a Protocol 1 connection keeps only the full-ring loss count.
    RadioConnection::TxSendStats p1;
    p1.valid = true;
    p1.overflowOnly = true;
    p1.overflowSamples = 7;
    const QString p1Line = DaemonMediaController::unkeyStatsLine("phone-1", rx, nullptr, p1);
    QVERIFY2(p1Line.contains(QStringLiteral("transmit I/Q lost 7 samples (no other counters)")),
             qPrintable(p1Line));
    QVERIFY2(!p1Line.contains(QStringLiteral("frames 0")), qPrintable(p1Line));
}

// TX diagnostics lane: the unkey line splits the padded silence and says
// when the first I/Q block went and where the longest mid-key silence fell;
// a line follows for each placed underrun (against the line's start and
// RF's), the first ran dry and each catch-up burst.
void TstDaemonMediaController::unkeyEventLinesPlaceTheOversDropouts()
{
    RemoteMicReceiver::Stats rx;
    RemoteMicFeed::Stats feed;
    feed.targetFrames = 1440;
    feed.underflows = 2;
    feed.underrunsPlacedCount = 2;
    // RF starts at 1000.0 + 69.0 ms on the steady clock (below). The first
    // underrun came in priming, the second mid-key and never recovered.
    feed.underrunsPlaced[0] = {12.0, 1'050'000, 8.0, -1.0, -1.0};
    feed.underrunsPlaced[1] = {2012.0, 3'081'000, -1.0, 520.0, 500.0};
    RadioConnection::TxSendStats send;
    send.valid = true;
    send.placed = true;
    send.framesSent = 5508;
    send.zeroPaddedSamples = 119'760;
    send.padStartSamples = 13'248;
    send.padMidSamples = 103'680;
    send.padTailSamples = 2'832;
    send.padTailAtMs = 4310.0;
    send.longestMidPadSamples = 103'680;
    send.longestMidPadAtMs = 2010.25;
    send.keySteadyNs = 1'000'000'000;
    send.firstBlockAtMs = 69.0;
    send.radioRanDry = 1;
    send.firstDryAtMs = 4120.5;
    send.firstDryGapMs = 21.0;
    send.catchUpBursts = 2;
    send.burstEvents = 2;
    send.bursts[0] = {69.0, 1.0, 8};
    send.bursts[1] = {4120.5, 21.0, 12};
    send.longestWakeGapMs = 512.0;
    send.longestWakeGapAtMs = 2005.5;
    send.wakeGapSequenceStep = 1;

    const QString line = DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, send);
    QVERIFY2(line.contains(QStringLiteral(
                 "silence 119760 samples (start 13248, mid-key 103680, unkey tail 2832 from "
                 "+4310.0 ms), late wakes 0")),
             qPrintable(line));
    QVERIFY2(line.endsWith(QStringLiteral(
                 ", first I/Q block at +69.0 ms of the key, longest mid-key silence 540.0 ms at "
                 "+2010.3 ms, longest wait for a microphone block 512.0 ms at +2005.5 ms of the "
                 "key, radio frame sequence step 1")),
             qPrintable(line));
    // Frames lost on the way: the step is the frames missed; a gap that
    // began just before the send thread's first keyed pass reads negative;
    // no sequence seen, and no wait measured, say so.
    RadioConnection::TxSendStats lost = send;
    lost.wakeGapSequenceStep = 385;
    lost.longestWakeGapAtMs = -2.5;
    QVERIFY2(DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, lost)
                 .endsWith(QStringLiteral("512.0 ms at -2.5 ms of the key, radio frame sequence "
                                          "step 385")),
             qPrintable(DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, lost)));
    lost.wakeGapSequenceStep = -1;
    QVERIFY(DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, lost)
                .endsWith(QStringLiteral("of the key, no radio frame sequence seen")));
    lost.longestWakeGapMs = -1.0;
    QVERIFY(DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, lost)
                .endsWith(QStringLiteral(", no wait for a microphone block measured")));

    const QStringList events = DaemonMediaController::unkeyEventLines("ab12", &feed, send);
    QCOMPARE(events.size(), 5);
    QCOMPARE(events.at(0),
             QStringLiteral("Transmit ended (ab12): microphone underrun 1 at +12.0 ms of the "
                            "line, 19.0 ms before RF started, silent 8.0 ms; no gap between "
                            "packets measured, no packet timestamps measured"));
    QCOMPARE(events.at(1),
             QStringLiteral("Transmit ended (ab12): microphone underrun 2 at +2012.0 ms of the "
                            "line and +2012.0 ms of RF, still silent at unkey; largest gap "
                            "between packets 520.0 ms, latest packet 500.0 ms behind its "
                            "timestamp"));
    QCOMPARE(events.at(2),
             QStringLiteral("Transmit ended (ab12): radio ran dry at +4120.5 ms of the key, "
                            "after a send gap of 21.0 ms"));
    QCOMPARE(events.at(3),
             QStringLiteral("Transmit ended (ab12): catch-up burst 1 at +69.0 ms of the key, 8 "
                            "frames after a send gap of 1.0 ms"));
    QCOMPARE(events.at(4),
             QStringLiteral("Transmit ended (ab12): catch-up burst 2 at +4120.5 ms of the key, "
                            "12 frames after a send gap of 21.0 ms"));

    // A placed path whose TX channel never sent: RF never started.
    RadioConnection::TxSendStats neverSent = send;
    neverSent.firstBlockAtMs = -1.0;
    QVERIFY2(DaemonMediaController::unkeyEventLines("ab12", &feed, neverSent)
                 .at(0)
                 .contains(QStringLiteral("of the line, RF never started, silent")),
             qPrintable(DaemonMediaController::unkeyEventLines("ab12", &feed, neverSent).at(0)));

    // Without a send path that places (Protocol 1), only the underruns,
    // and RF's start is not measured.
    RadioConnection::TxSendStats p1;
    p1.valid = true;
    p1.overflowOnly = true;
    const QStringList p1Events = DaemonMediaController::unkeyEventLines("ab12", &feed, p1);
    QCOMPARE(p1Events.size(), 2);
    QVERIFY2(p1Events.at(0).contains(QStringLiteral("of the line, RF start not measured, silent")),
             qPrintable(p1Events.at(0)));
    QVERIFY(!DaemonMediaController::unkeyStatsLine("ab12", rx, &feed, p1)
                 .contains(QStringLiteral("mid-key")));
    QVERIFY(DaemonMediaController::unkeyEventLines("ab12", nullptr, RadioConnection::TxSendStats{})
                .isEmpty());
}

QTEST_MAIN(TstDaemonMediaController)
#include "tst_daemon_media_controller.moc"

// R-R3-01/R-R3-08: E1 owns a Wide engine. E2 on the same stream asks for a
// longer Wide FFT, moves to Fine, resizes Fine and leaves. None of that may
// renew E1's context or change the engine that feeds it.
void TstDaemonMediaController::sharedEngineKeepsOtherPanWhileNeighbourChurns()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);

    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(1, 1, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 1).isEmpty();
    })());
    const QJsonObject e1 = messageFor(controls, QStringLiteral("context"), 1);
    const qint64 e1Generation = e1.value(QStringLiteral("contextGeneration")).toInteger();
    const double e1Span = e1.value(QStringLiteral("spanHz")).toDouble();

    const auto e1Untouched = [&]() {
        QCOMPARE(messageCount(controls, QStringLiteral("context"), 1), 1);
        const QJsonObject latest = messageFor(controls, QStringLiteral("context"), 1);
        QCOMPARE(latest.value(QStringLiteral("contextGeneration")).toInteger(), e1Generation);
        QCOMPARE(latest.value(QStringLiteral("spanHz")).toDouble(), e1Span);
    };
    const auto waitForE2Revision = [&](int revision) {
        QTRY_VERIFY(([&] {
            h.feedRadio();
            return messageFor(controls, QStringLiteral("context"), 2)
                .value(QStringLiteral("revision")).toInt() == revision;
        })());
    };

    // A "wide" label with a longer size cannot lengthen E1's engine.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 1, h.sliceId, centre, QStringLiteral("wide"), 4096),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    waitForE2Revision(1);
    // E2 shares E1's engine, so both crops cover the same bins.
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 2)
                 .value(QStringLiteral("spanHz")).toDouble(), e1Span);
    e1Untouched();

    // E2 moves to its own Fine engine, then resizes it.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 2, h.sliceId, centre, QStringLiteral("fine"), 8192),
        h.client.sessionEpoch()));
    waitForE2Revision(2);
    e1Untouched();
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 3, h.sliceId, centre, QStringLiteral("fine"), 16384),
        h.client.sessionEpoch()));
    waitForE2Revision(3);
    e1Untouched();

    QVERIFY(h.client.sendMediaControl(unsubscription(2), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_COMPARE(h.controller.activeSourceCount(), 1);

    // E1 still paints from its original context after the churn: a keyframe
    // is honoured only for the endpoint's current context generation.
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("keyframe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 1},
        {QStringLiteral("contextGeneration"), e1Generation}},
        h.client.sessionEpoch()));
    h.mediaTransport->displays.clear();
    QTRY_VERIFY(([&] {
        h.feedRadio();
        for (const QByteArray& bytes : std::as_const(h.mediaTransport->displays)) {
            DisplayCodecDecoder fresh;
            const DisplayCodecDecodeResult decoded = fresh.decode(bytes);
            if (decoded.disposition == DisplayCodecDisposition::Accepted
                && decoded.frame.context.endpointId == 1
                && decoded.frame.context.contextGeneration
                    == static_cast<quint32>(e1Generation)) {
                return true;
            }
        }
        return false;
    })());
    e1Untouched();

    // The reverse: a pan that only shares E1's engine leaves, and E1 is
    // still untouched.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(3, 1, h.sliceId, centre, QStringLiteral("wide"), 4096),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageCount(controls, QStringLiteral("context"), 3) == 1;
    })());
    QCOMPARE(h.controller.spectrumGrant(3)->grantedFftSize, 1024);
    QCOMPARE(h.controller.spectrumGrant(3)->reason, SpectrumLimitReason::SharedEngine);
    QVERIFY(h.client.sendMediaControl(unsubscription(3), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    for (int i = 0; i < 20; ++i) { h.feedRadio(); QTest::qWait(10); }
    e1Untouched();

    // E1 leaves. The pan that was held to E1's engine is now alone on it,
    // so it is granted its own request, and its renewed context says so.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(4, 1, h.sliceId, centre, QStringLiteral("wide"), 4096),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageCount(controls, QStringLiteral("context"), 4) == 1;
    })());
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 4)
                 .value(QStringLiteral("limit")).toString(), QStringLiteral("shared"));
    QVERIFY(h.client.sendMediaControl(unsubscription(1), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageCount(controls, QStringLiteral("context"), 4) == 2;
    })());
    const QJsonObject upgraded = messageFor(controls, QStringLiteral("context"), 4);
    QCOMPARE(upgraded.value(QStringLiteral("grantedFftSize")).toInt(), 4096);
    QCOMPARE(upgraded.value(QStringLiteral("limit")).toString(), QStringLiteral("none"));
    QCOMPARE(h.controller.spectrumGrant(4)->grantedFftSize, 4096);
    QCOMPARE(h.controller.spectrumGrant(4)->reason, SpectrumLimitReason::None);
    h.finish();
}

// R-R3-01/R-R3-08: a neighbour's frame rate is not a reason to renew a pan.
// The engine runs at the highest rate its pans ask for; each endpoint keeps
// its own cadence, so no pan is held below the rate it requested and no pan
// is renewed when a neighbour joins, changes rate or leaves.
void TstDaemonMediaController::sharedEngineKeepsOtherPanAcrossFrameRates()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const auto atFps = [&](quint32 endpointId, quint32 revision, int fps) {
        QJsonObject request = tieredSubscription(endpointId, revision, h.sliceId, centre,
                                                 QStringLiteral("wide"), 1024);
        request.insert(QStringLiteral("fps"), fps);
        return request;
    };
    const auto contextFor = [&](quint32 endpointId, int count) {
        QTRY_VERIFY(([&] {
            h.feedRadio();
            return messageCount(controls, QStringLiteral("context"), endpointId) >= count;
        })());
    };
    const auto settle = [&] {
        for (int i = 0; i < 20; ++i) { h.feedRadio(); QTest::qWait(10); }
    };

    // E1 at 30 fps alone, then E2 on the same engine at 60, then at 15,
    // then gone. E1 keeps one context and its bins throughout.
    QVERIFY(h.client.sendMediaControl(atFps(1, 1, 30), h.client.sessionEpoch()));
    contextFor(1, 1);
    const QJsonObject e1 = messageFor(controls, QStringLiteral("context"), 1);
    const auto e1Untouched = [&] {
        QCOMPARE(messageCount(controls, QStringLiteral("context"), 1), 1);
        const QJsonObject latest = messageFor(controls, QStringLiteral("context"), 1);
        QCOMPARE(latest.value(QStringLiteral("contextGeneration")),
                 e1.value(QStringLiteral("contextGeneration")));
        QCOMPARE(latest.value(QStringLiteral("traceSamples")),
                 e1.value(QStringLiteral("traceSamples")));
        QCOMPARE(latest.value(QStringLiteral("fps")).toInt(), 30);
    };
    QCOMPARE(h.controller.spectrumSourceFps(1), std::optional<int>(30));

    QVERIFY(h.client.sendMediaControl(atFps(2, 1, 60), h.client.sessionEpoch()));
    contextFor(2, 1);
    settle();
    e1Untouched();
    QCOMPARE(h.controller.spectrumSourceFps(2), std::optional<int>(60));
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 2)
                 .value(QStringLiteral("fps")).toInt(), 60);

    QVERIFY(h.client.sendMediaControl(atFps(2, 2, 15), h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 2)
            .value(QStringLiteral("revision")).toInt() == 2;
    })());
    settle();
    e1Untouched();
    QCOMPARE(h.controller.spectrumSourceFps(1), std::optional<int>(30));

    QVERIFY(h.client.sendMediaControl(unsubscription(2), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    settle();
    e1Untouched();

    // The reverse order: a background pan at 15 fps is first, a focused pan
    // at 60 joins. The focused pan's engine runs at its own rate, and the
    // background pan is not renewed when the focused pan arrives or leaves.
    QVERIFY(h.client.sendMediaControl(unsubscription(1), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 0);
    QVERIFY(h.client.sendMediaControl(atFps(3, 1, 15), h.client.sessionEpoch()));
    contextFor(3, 1);
    QVERIFY(h.client.sendMediaControl(atFps(4, 1, 60), h.client.sessionEpoch()));
    contextFor(4, 1);
    settle();
    QCOMPARE(h.controller.spectrumSourceFps(4), std::optional<int>(60));
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 4)
                 .value(QStringLiteral("fps")).toInt(), 60);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 3), 1);
    QVERIFY(h.client.sendMediaControl(unsubscription(4), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    settle();
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 3), 1);
    QCOMPARE(h.controller.spectrumSourceFps(3), std::optional<int>(15));
    h.finish();
}

// R-R3-01/R-R3-08: the grant records what Core gave each request and why.
void TstDaemonMediaController::grantReportsLargestSizeSharedEngineAndSourceBins()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const int largest = NereusSDR::FFTEngine::maximumFftSize();

    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(1, 1, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    auto grant = h.controller.spectrumGrant(1);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->requestedFftSize, 1024);
    QCOMPARE(grant->grantedFftSize, 1024);
    QCOMPARE(grant->grantedTier, FftTier::Wide);
    QCOMPARE(grant->requestedPixels, 128);
    QCOMPARE(grant->grantedPixels, 128);
    QCOMPARE(grant->reason, SpectrumLimitReason::None);
    QVERIFY(!h.controller.spectrumGrant(99).has_value());

    // Above the largest size on an engine another pan uses: the shared
    // engine is the limit that decides the grant.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 1, h.sliceId, centre, QStringLiteral("wide"), largest * 2),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    grant = h.controller.spectrumGrant(2);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->requestedFftSize, largest * 2);
    QCOMPARE(grant->grantedFftSize, 1024);
    QCOMPARE(grant->reason, SpectrumLimitReason::SharedEngine);

    // Alone on its own Fine engine, the same request gets the largest size.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(3, 1, h.sliceId, centre, QStringLiteral("fine"), largest * 2),
        h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 3);
    grant = h.controller.spectrumGrant(3);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->grantedFftSize, largest);
    QCOMPARE(grant->grantedTier, FftTier::Fine);
    QCOMPARE(grant->reason, SpectrumLimitReason::LargestSize);

    // More pixels than the crop has source bins: granted the visible bins,
    // and the context carries exactly that many samples.
    QJsonObject wider = tieredSubscription(1, 2, h.sliceId, centre,
                                           QStringLiteral("wide"), 1024);
    wider.insert(QStringLiteral("pixels"), SpectrumEndpoint::kMaxPixels);
    QVERIFY(h.client.sendMediaControl(wider, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.spectrumGrant(1)->requestedPixels, SpectrumEndpoint::kMaxPixels);
    grant = h.controller.spectrumGrant(1);
    QCOMPARE(grant->grantedFftSize, 1024);
    QCOMPARE(grant->reason, SpectrumLimitReason::SourceBins);
    // 48 kHz of a 192 kHz, 1024-bin source: 256 bins plus the inclusive edge.
    QCOMPARE(grant->grantedPixels, 257);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 1)
            .value(QStringLiteral("revision")).toInt() == 2;
    })());
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 1)
                 .value(QStringLiteral("traceSamples")).toInt(), 257);
    QCOMPARE(h.controller.spectrumGrant(1)->grantedPixels, 257);
    h.finish();
}

// R-R3-08: the display budget charges the pixels Core grants, not the ones
// asked for, so a request clamped by its source bins fits a tight budget.
void TstDaemonMediaController::budgetChargesGrantedPixels()
{
    const SpectrumDisplayCost limit = *spectrumDisplayCost(128, 60, false);
    Harness h(DisplayBudgetLimits{limit.charge.applicationBytesPerSecond,
                                  limit.charge.spectrumSampleUnitsPerSecond, 7});
    h.establishSession();
    QVERIFY(h.server.displayBudgetAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();

    QJsonObject request = subscription(70, 1, h.sliceId,
                                       h.radio.streamCentreHz(h.streamIndex));
    request.insert(QStringLiteral("pixels"), SpectrumEndpoint::kMaxPixels);
    request.insert(QStringLiteral("spanHz"), 18750.0); // about 100 source bins
    QVERIFY(h.client.sendMediaControl(request, h.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 70, 1).isEmpty());
    const QJsonObject result = allocationFor(controls, 70, 1);
    QCOMPARE(result.value(QStringLiteral("accepted")).toBool(), true);
    const auto grant = h.controller.spectrumGrant(70);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->reason, SpectrumLimitReason::SourceBins);
    QVERIFY(grant->grantedPixels > 0 && grant->grantedPixels < 128);
    const SpectrumDisplayCost charged = *spectrumDisplayCost(grant->grantedPixels, 60, false);
    QCOMPARE(result.value(QStringLiteral("spectrumSampleUnitsPerSecond")).toInteger(),
             static_cast<qint64>(charged.charge.spectrumSampleUnitsPerSecond));
    QCOMPARE(result.value(QStringLiteral("applicationBytesPerSecond")).toInteger(),
             static_cast<qint64>(charged.charge.applicationBytesPerSecond));
    h.finish();
}

// R-R3-01/R-R3-08/R-R3-37: under the display budget a minor 9 GUI holds
// the charge for the pixels it was granted. When the neighbour that held it
// to a smaller engine leaves, the pan gets its own FFT size but no pixels
// beyond that charge, and it is not told the receiver lacks detail.
void TstDaemonMediaController::regrantAfterNeighbourLeavesStaysWithinAdmittedCharge()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 7});
    h.establishSession();
    QVERIFY(h.server.displayBudgetAvailable());
    QVERIFY(h.server.spectrumGrantAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);

    QVERIFY(h.client.sendMediaControl(subscription(1, 1, h.sliceId, centre),
                                      h.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 1, 1).isEmpty());
    QJsonObject deep = tieredSubscription(2, 1, h.sliceId, centre,
                                          QStringLiteral("wide"), 4096);
    deep.insert(QStringLiteral("spanHz"), 6000.0);
    QVERIFY(h.client.sendMediaControl(deep, h.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 2, 1).isEmpty());
    QVERIFY(allocationFor(controls, 2, 1).value(QStringLiteral("accepted")).toBool());
    const auto shared = h.controller.spectrumGrant(2);
    QVERIFY(shared.has_value());
    QCOMPARE(shared->reason, SpectrumLimitReason::SharedEngine);
    QCOMPARE(shared->grantedFftSize, 1024);
    const int admittedPixels = shared->grantedPixels;
    QVERIFY(admittedPixels > 0 && admittedPixels < 128);
    const SpectrumDisplayCost admitted = *spectrumDisplayCost(admittedPixels, 60, false);
    QCOMPARE(allocationFor(controls, 2, 1).value(QStringLiteral("spectrumSampleUnitsPerSecond"))
                 .toInteger(),
             static_cast<qint64>(admitted.charge.spectrumSampleUnitsPerSecond));

    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("unsubscribe")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("endpointId"), 1},
        {QStringLiteral("revision"), 2}}, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 2)
            .value(QStringLiteral("grantedFftSize")).toInt() == 4096;
    })());
    const QJsonObject context = messageFor(controls, QStringLiteral("context"), 2);
    QCOMPARE(context.value(QStringLiteral("traceSamples")).toInt(), admittedPixels);
    QCOMPARE(context.value(QStringLiteral("limit")).toString(), QStringLiteral("none"));
    const auto regranted = h.controller.spectrumGrant(2);
    QCOMPARE(regranted->grantedFftSize, 4096);
    QCOMPARE(regranted->grantedPixels, admittedPixels);
    QCOMPARE(regranted->reason, SpectrumLimitReason::None);
    h.finish();
}

// R-R3-09: Core accepts only what the GUI's context parser accepts. Every
// refused request goes through the typed rejection path and leaves the live
// endpoint and its source exactly as they were.
void TstDaemonMediaController::outOfRangeRequestsAreRejectedAndLeaveEndpointUntouched()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const QJsonObject live = subscription(1, 1, h.sliceId, centre);
    QVERIFY(h.client.sendMediaControl(live, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 1).isEmpty();
    })());
    const qint64 generation = messageFor(controls, QStringLiteral("context"), 1)
        .value(QStringLiteral("contextGeneration")).toInteger();

    const QList<std::pair<QString, QJsonValue>> invalid{
        {QStringLiteral("framesPerLine"), kMaxFramesPerLine + 1},
        {QStringLiteral("minDbm"), kMinDbmLimit - 1.0},
        {QStringLiteral("maxDbm"), kMaxDbmLimit + 1.0},
        {QStringLiteral("centreHz"), centre + 500000.0},
        {QStringLiteral("fps"), 0},
        {QStringLiteral("fps"), 61},
        {QStringLiteral("pixels"), 0},
        {QStringLiteral("pixels"), SpectrumEndpoint::kMaxPixels + 1},
        {QStringLiteral("spanHz"), 0.0},
        {QStringLiteral("spanHz"), -48000.0},
        {QStringLiteral("fftSize"), 3000},
        {QStringLiteral("fps"), QStringLiteral("60")},
        {QStringLiteral("minDbm"), 0.0}, // equal to maxDbm
    };
    quint32 endpointId = 10;
    for (const auto& [key, value] : invalid) {
        QJsonObject bad = subscription(endpointId, 1, h.sliceId, centre);
        bad.insert(key, value);
        QVERIFY(h.client.sendMediaControl(bad, h.client.sessionEpoch()));
        QTRY_VERIFY2(!messageFor(controls, QStringLiteral("rejected"), endpointId).isEmpty(),
                     qPrintable(key));
        QCOMPARE(h.controller.activeEndpointCount(), 1);
        QCOMPARE(h.controller.activeSourceCount(), 1);
        ++endpointId;

        // The same bad value as a replacement of the live endpoint.
        QJsonObject replacement = live;
        replacement.insert(QStringLiteral("revision"), static_cast<qint64>(endpointId));
        replacement.insert(key, value);
        const int rejectedBefore = messageCount(controls, QStringLiteral("rejected"), 1);
        QVERIFY(h.client.sendMediaControl(replacement, h.client.sessionEpoch()));
        QTRY_COMPARE(messageCount(controls, QStringLiteral("rejected"), 1), rejectedBefore + 1);
        QCOMPARE(h.controller.activeEndpointCount(), 1);
        QCOMPARE(h.controller.spectrumGrant(1)->requestedPixels, 128);
    }

    h.feedRadio(0.1875);
    QTest::qWait(20);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 1), 1);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 1)
                 .value(QStringLiteral("contextGeneration")).toInteger(), generation);

    // The limits themselves are accepted, and the GUI accepts their context.
    QJsonObject edge = subscription(40, 1, h.sliceId, centre);
    edge.insert(QStringLiteral("framesPerLine"), kMaxFramesPerLine);
    edge.insert(QStringLiteral("minDbm"), kMinDbmLimit);
    edge.insert(QStringLiteral("maxDbm"), kMaxDbmLimit);
    QVERIFY(h.client.sendMediaControl(edge, h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QVERIFY(messageFor(controls, QStringLiteral("rejected"), 40).isEmpty());
    h.finish();
}

// Ownership is pinned: a control from another epoch or a connection id that
// is not the session's media peer never reaches the endpoints.
void TstDaemonMediaController::staleEpochAndForeignConnectionLeaveEndpointsUntouched()
{
    Harness h;
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const quint64 epoch = h.client.sessionEpoch();
    QVERIFY(h.client.sendMediaControl(subscription(1, 1, h.sliceId, centre), epoch));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !messageFor(controls, QStringLiteral("context"), 1).isEmpty();
    })());
    const QJsonObject context = messageFor(controls, QStringLiteral("context"), 1);

    // Delivered as the station would, but stamped with a different epoch.
    QJsonObject replacement = subscription(1, 2, h.sliceId, centre, 4096);
    emit h.server.mediaControlReceived(replacement, epoch + 1);
    emit h.server.mediaControlReceived(unsubscription(1), epoch + 1);
    emit h.server.mediaControlReceived(subscription(2, 1, h.sliceId, centre), epoch + 1);
    if (epoch > 1) {
        emit h.server.mediaControlReceived(unsubscription(1), epoch - 1);
    }

    // A syntactically valid connection id that is not this session's peer.
    const QString foreign = QStringLiteral("99999999-2222-4333-8444-555555555555");
    replacement.insert(QStringLiteral("connectionId"), foreign);
    QVERIFY(h.client.sendMediaControl(replacement, epoch));
    QJsonObject foreignUnsubscribe = unsubscription(1);
    foreignUnsubscribe.insert(QStringLiteral("connectionId"), foreign);
    QVERIFY(h.client.sendMediaControl(foreignUnsubscribe, epoch));
    QJsonObject foreignNew = subscription(3, 1, h.sliceId, centre);
    foreignNew.insert(QStringLiteral("connectionId"), foreign);
    QVERIFY(h.client.sendMediaControl(foreignNew, epoch));

    // Positive control: the same direct delivery with the live epoch works.
    emit h.server.mediaControlReceived(subscription(4, 1, h.sliceId, centre), epoch);
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QVERIFY(!h.controller.spectrumGrant(2).has_value());
    QVERIFY(!h.controller.spectrumGrant(3).has_value());
    QCOMPARE(h.controller.spectrumGrant(1)->requestedFftSize, 1024);

    h.feedRadio(0.1875);
    QTest::qWait(20);
    QCOMPARE(messageCount(controls, QStringLiteral("context"), 1), 1);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 1)
                 .value(QStringLiteral("contextGeneration")).toInteger(),
             context.value(QStringLiteral("contextGeneration")).toInteger());
    QVERIFY(messageFor(controls, QStringLiteral("rejected"), 1).isEmpty());
    h.finish();
}

// R-R3-09: a GUI from before minor 9 keeps receiving exactly today's
// spectrum context, with and without wideband, while Core still records
// the grant it made.
void TstDaemonMediaController::minorEightPeerReceivesTodaysSpectrumContext()
{
    Harness h;
    h.enableWidebandSource();
    auto* station = new Test::LoopbackTransport(QStringLiteral("minor8-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("minor8-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteSpectrumGrantSessionProtocolMinor - 1, 0,
        QStringLiteral("minor-8 client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(h.server.remoteWidebandAvailable());
    QVERIFY(!h.server.spectrumGrantAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    send(subscription(81, 1, h.sliceId, centre));
    QJsonObject extended = subscription(82, 1, h.sliceId, centre);
    extended.insert(QStringLiteral("extendedView"), true);
    send(extended);
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);

    const auto contextFor = [&](quint32 endpointId) {
        QJsonObject latest;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::MediaControl
                && message.mediaPayload.value(QStringLiteral("op")) == QLatin1String("context")
                && message.mediaPayload.value(QStringLiteral("endpointId")).toInteger()
                    == qint64{endpointId}) {
                latest = message.mediaPayload;
            }
        }
        return latest;
    };
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !contextFor(81).isEmpty() && !contextFor(82).isEmpty();
    })());

    QStringList todaysKeys{
        QStringLiteral("op"), QStringLiteral("connectionId"), QStringLiteral("endpointId"),
        QStringLiteral("revision"), QStringLiteral("contextGeneration"),
        QStringLiteral("sourceStream"), QStringLiteral("sourceCentreHz"),
        QStringLiteral("sampleRateHz"), QStringLiteral("centreHz"), QStringLiteral("spanHz"),
        QStringLiteral("wideCentreHz"), QStringLiteral("wideSpanHz"),
        QStringLiteral("traceSamples"), QStringLiteral("waterfallSamples"),
        QStringLiteral("wideSamples"), QStringLiteral("minDbm"), QStringLiteral("maxDbm"),
        QStringLiteral("fps"), QStringLiteral("framesPerLine")};
    QCOMPARE(todaysKeys.size(), 19);
    for (const quint32 endpointId : {81u, 82u}) {
        const QJsonObject context = contextFor(endpointId);
        QStringList expected = todaysKeys;
        if (endpointId == 82) { expected.append(QStringLiteral("wideband")); }
        expected.sort();
        QCOMPARE(context.keys(), expected);
        // What a minor-8 GUI accepts, and not what a minor-9 GUI accepts.
        const std::optional<SpectrumContextMessage> decoded =
            decodeRemoteSpectrumContext(context, false);
        QVERIFY(decoded.has_value());
        QVERIFY(!decodeRemoteSpectrumContext(context, true).has_value());
        QCOMPARE(encodeRemoteSpectrumContext(*decoded, false), context);
        QVERIFY(h.controller.spectrumGrant(endpointId).has_value());
    }
    peer->closeLink(QStringLiteral("test complete"));
}

// R-R3-01: a minor-9 GUI talking to today's Core (which negotiates down to
// minor 9) still receives the grant in each spectrum context, with and
// without wideband.
void TstDaemonMediaController::minorNinePeerReceivesTheGrant()
{
    QVERIFY(kSessionProtocolMinor > kRemoteSpectrumGrantSessionProtocolMinor);
    Harness h;
    h.enableWidebandSource();
    auto* station = new Test::LoopbackTransport(QStringLiteral("minor9-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("minor9-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteSpectrumGrantSessionProtocolMinor, 0,
        QStringLiteral("minor-9 client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(h.server.remoteWidebandAvailable());
    QVERIFY(h.server.spectrumGrantAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    send(subscription(91, 1, h.sliceId, centre));
    QJsonObject extended = subscription(92, 1, h.sliceId, centre);
    extended.insert(QStringLiteral("extendedView"), true);
    send(extended);
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);

    const auto contextFor = [&](quint32 endpointId) {
        QJsonObject latest;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::MediaControl
                && message.mediaPayload.value(QStringLiteral("op")) == QLatin1String("context")
                && message.mediaPayload.value(QStringLiteral("endpointId")).toInteger()
                    == qint64{endpointId}) {
                latest = message.mediaPayload;
            }
        }
        return latest;
    };
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return !contextFor(91).isEmpty() && !contextFor(92).isEmpty();
    })());

    for (const quint32 endpointId : {91u, 92u}) {
        const QJsonObject context = contextFor(endpointId);
        QVERIFY(context.contains(QStringLiteral("grantedFftSize")));
        QVERIFY(context.contains(QStringLiteral("grantedPixels")));
        QCOMPARE(context.contains(QStringLiteral("wideband")), endpointId == 92);
        // What a minor-9 GUI accepts, and not what a minor-8 GUI accepts.
        const std::optional<SpectrumContextMessage> decoded =
            decodeRemoteSpectrumContext(context, true);
        QVERIFY(decoded.has_value());
        QVERIFY(!decodeRemoteSpectrumContext(context, false).has_value());
        QVERIFY(decoded->grant.has_value());
        const auto recorded = h.controller.spectrumGrant(endpointId);
        QVERIFY(recorded.has_value());
        QCOMPARE(*decoded->grant, spectrumContextGrant(*recorded));
    }
    peer->closeLink(QStringLiteral("test complete"));
}

// R-R3-01/08: from minor 9 on, each context carries the grant Core recorded,
// including what limited it. The harness negotiates the current minor, which
// only has to have reached the grant minor; later minors keep the grant.
// Parity Task 17 (R-R3-01, B3.7): spectrumGrantVersion 2's `decimation`
// reaches the endpoint's own engine. Beside another pan it runs at the
// engine's decimation (no pan's spectrum changes to satisfy another's
// request), and a pan left alone gets its own. A value outside 1 to 16 is
// a request the Core cannot read.
void TstDaemonMediaController::subscribeDecimationReachesTheEndpointsEngine()
{
    Harness h;
    h.establishSession();
    QCOMPARE(h.server.buildCapabilities().spectrumGrantVersion, 2);
    QVERIFY(h.client.spectrumDecimationAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const auto withDecimation = [&](quint32 endpointId, quint32 revision, const QJsonValue& d) {
        QJsonObject request = tieredSubscription(endpointId, revision, h.sliceId, centre,
                                                 QStringLiteral("wide"), 1024);
        request.insert(QStringLiteral("decimation"), d);
        return request;
    };
    const auto contextRevision = [&](quint32 endpointId) {
        return messageFor(controls, QStringLiteral("context"), endpointId)
            .value(QStringLiteral("revision")).toInt();
    };

    // Wrong: outside 1 to 16, or not a whole number.
    int refusals = 0;
    for (const QJsonValue& bad : {QJsonValue(0), QJsonValue(17), QJsonValue(32), QJsonValue(2.5),
                                  QJsonValue(QStringLiteral("4"))}) {
        QVERIFY(h.client.sendMediaControl(withDecimation(9, quint32(refusals + 1), bad),
                                          h.client.sessionEpoch()));
        ++refusals;
        QTRY_COMPARE(messageCount(controls, QStringLiteral("rejected"), 9), refusals);
        QCOMPARE(messageFor(controls, QStringLiteral("rejected"), 9)
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core could not read this display request."));
    }
    QCOMPARE(h.controller.activeEndpointCount(), 0);

    // Right: alone on its engine, the engine takes it.
    QVERIFY(h.client.sendMediaControl(withDecimation(1, 1, 4), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_VERIFY(([&] { h.feedRadio(); return contextRevision(1) == 1; })());
    QTRY_COMPARE(h.controller.spectrumSourceDecimation(1).value_or(0), 4);
    // Asked again with another: the engine follows.
    QVERIFY(h.client.sendMediaControl(withDecimation(1, 2, 2), h.client.sessionEpoch()));
    QTRY_VERIFY(([&] { h.feedRadio(); return contextRevision(1) == 2; })());
    QTRY_COMPARE(h.controller.spectrumSourceDecimation(1).value_or(0), 2);

    // A second pan on the same engine asking for 8 runs at the engine's 2.
    QVERIFY(h.client.sendMediaControl(withDecimation(2, 1, 8), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
    QTRY_VERIFY(([&] { h.feedRadio(); return contextRevision(2) == 1; })());
    QTest::qWait(50);
    QCOMPARE(h.controller.spectrumSourceDecimation(2).value_or(0), 2);
    QCOMPARE(h.controller.spectrumSourceDecimation(1).value_or(0), 2);
    // Parity Task 17 follow-up: the window is told its pan runs at the
    // shared engine's decimation (limit "shared", the reason a shared
    // engine's size gives); the first pan runs at what it asked for.
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 2)
                 .value(QStringLiteral("limit")).toString(), QStringLiteral("shared"));
    QCOMPARE(int(h.controller.spectrumGrant(2).value_or(SpectrumGrant{}).reason),
             int(SpectrumLimitReason::SharedEngine));
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 1)
                 .value(QStringLiteral("limit")).toString(), QStringLiteral("none"));

    // The first pan leaves: the one left gets the 8 it asked for, and its
    // renewed context no longer says it is held.
    QVERIFY(h.client.sendMediaControl(unsubscription(1), h.client.sessionEpoch()));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QTRY_COMPARE(h.controller.spectrumSourceDecimation(2).value_or(0), 8);
    QTRY_VERIFY(([&] {
        h.feedRadio();
        return messageFor(controls, QStringLiteral("context"), 2)
                   .value(QStringLiteral("limit")).toString() == QLatin1String("none");
    })());

    // Without the field a request runs undecimated.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 2, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    QTRY_VERIFY(([&] { h.feedRadio(); return contextRevision(2) == 2; })());
    QTRY_COMPARE(h.controller.spectrumSourceDecimation(2).value_or(0), 1);
    h.finish();
}

// Parity Task 17 follow-up (R-R3-01): `decimation` came with
// spectrumGrantVersion 2, which the Core tells only a peer at the grant
// minor. A peer below it that sends one anyway is refused, while the same
// request without it is taken.
void TstDaemonMediaController::peerBelowTheGrantMinorCannotAskForDecimation()
{
    Harness h;
    auto* station = new Test::LoopbackTransport(QStringLiteral("minor8-station"), this);
    auto* peer = new Test::LoopbackTransport(QStringLiteral("minor8-peer"), this);
    station->linkTo(peer);
    h.server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kRemoteSpectrumGrantSessionProtocolMinor - 1, 0,
        QStringLiteral("minor-8 client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(h.server.token())));
    QTRY_VERIFY(h.server.mediaAvailable());
    QVERIFY(!h.server.spectrumGrantAvailable());
    const auto send = [&](const QJsonObject& payload) {
        SessionMessage message;
        message.kind = SessionMessageKind::MediaControl;
        message.mediaPayload = payload;
        peer->sendText(SessionMessages::encode(message));
    };
    send({{QStringLiteral("op"), QStringLiteral("start")},
          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}});
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    QJsonObject decimated = subscription(83, 1, h.sliceId, centre);
    decimated.insert(QStringLiteral("decimation"), 4);
    send(decimated);
    send(subscription(84, 1, h.sliceId, centre));
    QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
    QVERIFY(!h.controller.spectrumGrant(83).has_value());
    QVERIFY(h.controller.spectrumGrant(84).has_value());
    QCOMPARE(h.controller.spectrumSourceDecimation(84).value_or(0), 1);
    peer->closeLink(QStringLiteral("test complete"));
}

void TstDaemonMediaController::currentMinorSpectrumContextsReportTheGrant()
{
    Harness h;
    h.establishSession();
    QCOMPARE(h.client.agreedMinor(), kSessionProtocolMinor);
    QVERIFY(h.client.agreedMinor() >= kRemoteSpectrumGrantSessionProtocolMinor);
    QVERIFY(h.server.spectrumGrantAvailable());
    QVERIFY(h.client.spectrumGrantAvailable());
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer();
    const double centre = h.radio.streamCentreHz(h.streamIndex);
    const int largest = NereusSDR::FFTEngine::maximumFftSize();

    const auto reported = [&](quint32 endpointId, quint32 revision)
        -> std::optional<SpectrumContextGrant> {
        const QJsonObject context = messageFor(controls, QStringLiteral("context"), endpointId);
        if (context.value(QStringLiteral("revision")).toInteger() != qint64{revision}) {
            return std::nullopt;
        }
        const std::optional<SpectrumContextMessage> decoded =
            decodeRemoteSpectrumContext(context, true);
        return decoded ? decoded->grant : std::nullopt;
    };
    const auto awaitReport = [&](quint32 endpointId, quint32 revision, int feedsPerPoll) {
        std::optional<SpectrumContextGrant> grant;
        // The caller verifies the result; QTRY cannot return a value.
        const bool arrived = QTest::qWaitFor([&] {
            for (int feed = 0; feed < feedsPerPoll; ++feed) { h.feedRadio(); }
            grant = reported(endpointId, revision);
            return grant.has_value();
        }, 10000);
        Q_UNUSED(arrived);
        return grant;
    };

    // Alone on its engine: nothing limits it.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(1, 1, h.sliceId, centre, QStringLiteral("wide"), 1024),
        h.client.sessionEpoch()));
    std::optional<SpectrumContextGrant> grant = awaitReport(1, 1, 1);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->grantedFftSize, 1024);
    QCOMPARE(grant->grantedTier, FftTier::Wide);
    QCOMPARE(grant->requestedPixels, 128);
    QCOMPARE(grant->grantedPixels, 128);
    QCOMPARE(grant->limit, SpectrumLimitReason::None);
    QCOMPARE(*grant, spectrumContextGrant(*h.controller.spectrumGrant(1)));

    // A larger request on the Wide engine E1 uses: the shared engine stands.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(2, 1, h.sliceId, centre, QStringLiteral("wide"), largest * 2),
        h.client.sessionEpoch()));
    grant = awaitReport(2, 1, 1);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->grantedFftSize, 1024);
    QCOMPARE(grant->limit, SpectrumLimitReason::SharedEngine);
    QCOMPARE(*grant, spectrumContextGrant(*h.controller.spectrumGrant(2)));

    // Alone on the Fine engine, above the largest size.
    QVERIFY(h.client.sendMediaControl(
        tieredSubscription(3, 1, h.sliceId, centre, QStringLiteral("fine"), largest * 2),
        h.client.sessionEpoch()));
    grant = awaitReport(3, 1, 64);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->grantedFftSize, largest);
    QCOMPARE(grant->grantedTier, FftTier::Fine);
    QCOMPARE(grant->limit, SpectrumLimitReason::LargestSize);
    QCOMPARE(*grant, spectrumContextGrant(*h.controller.spectrumGrant(3)));

    // More points than E1's crop has source bins.
    QJsonObject wider = tieredSubscription(1, 2, h.sliceId, centre, QStringLiteral("wide"), 1024);
    wider.insert(QStringLiteral("pixels"), SpectrumEndpoint::kMaxPixels);
    QVERIFY(h.client.sendMediaControl(wider, h.client.sessionEpoch()));
    grant = awaitReport(1, 2, 1);
    QVERIFY(grant.has_value());
    QCOMPARE(grant->requestedPixels, SpectrumEndpoint::kMaxPixels);
    QCOMPARE(grant->grantedPixels, 257);
    QCOMPARE(grant->limit, SpectrumLimitReason::SourceBins);
    QCOMPARE(messageFor(controls, QStringLiteral("context"), 1)
                 .value(QStringLiteral("traceSamples")).toInt(), 257);
    QCOMPARE(*grant, spectrumContextGrant(*h.controller.spectrumGrant(1)));
    QVERIFY(messageFor(controls, QStringLiteral("rejected"), 1).isEmpty());
    h.finish();
}


namespace {

// R-R3-45: the start of a GUI that understands audio profiles and plays the
// headphones mix.
QJsonObject headphonesStart()
{
    QJsonObject start = profileStart();
    start.insert(QStringLiteral("headphonesMixVersion"), 1);
    return start;
}

QJsonObject headphonesAudioControl(quint32 revision, bool enabled,
                                   const QString& profile = QStringLiteral("opus"))
{
    return {{QStringLiteral("op"), QStringLiteral("headphones-audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("profile"), profile}};
}

QList<QJsonObject> headphonesContextsIn(const QSignalSpy& controls)
{
    QList<QJsonObject> found;
    for (const auto& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("headphones-audio-context")) {
            found.append(message);
        }
    }
    return found;
}

// The largest sample magnitude in lossless packets on `ssrc`.
float peakOfL16(const QList<QByteArray>& packets, quint32 ssrc)
{
    float peak = 0.0f;
    for (const QByteArray& packet : packets) {
        const PcmRtpDecodeResult l16 = decodeL16Rtp(packet, ssrc);
        if (l16.status != OpusAudioCodecStatus::Accepted) {
            return -1.0f;
        }
        for (float sample : l16.pcmInterleaved) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    return peak;
}

// Both of the harness's slices back on the speakers, here and in the
// settings they saved, so later tests start from the default.
void resetOutputRoutes(Harness& h)
{
    for (int sliceId : {h.sliceId, h.spareSliceId}) {
        if (SliceModel* slice = h.radio.sliceById(sliceId)) {
            slice->setOutputRoute(SliceModel::OutputRoute::Speakers);
        }
        AppSettings::instance().remove(QStringLiteral("Slice%1/OutputRoute").arg(sliceId));
    }
}

} // namespace

// R-R3-45: an app that did not declare headphonesMixVersion at start sees
// today's wire: no headphones stream id in the offer, a headphones request
// ignored, and a receiver routed to the headphones still in its one mix.
void TstDaemonMediaController::olderAppGetsTheWholeProgramAndNoHeadphonesMix()
{
    Harness h;
    const auto routes = qScopeGuard([&h] { resetOutputRoutes(h); });
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    QCOMPARE(h.server.buildCapabilities().headphonesMixVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(
                 h.server.buildCapabilities().toUpdates()).headphonesMixVersion, 1);
    QCOMPARE(h.client.capabilities().headphonesMixVersion, 1);

    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    // A malformed capability version starts nothing.
    for (const QJsonValue& bad : {QJsonValue(0), QJsonValue(1.5), QJsonValue(-1),
                                  QJsonValue(QStringLiteral("1")), QJsonValue()}) {
        QJsonObject start = headphonesStart();
        start.insert(QStringLiteral("headphonesMixVersion"), bad);
        QVERIFY(h.client.sendMediaControl(start, h.client.sessionEpoch()));
        QTest::qWait(20);
        QVERIFY(!h.mediaTransport);
    }
    QVERIFY(h.client.sendMediaControl(profileStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    QCOMPARE(h.mediaTransport->startOptions.headphonesAudioSsrc, quint32{0});
    QVERIFY(h.mediaTransport->startOptions.receiverAudioSsrcs.isEmpty());
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();
    const quint32 mainSsrc = h.mediaTransport->startOptions.localAudioSsrc;
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);

    // B on the headphones: the one mix still carries it.
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    feedSlicesBlock(h, {{h.sliceId, 0.0f}, {h.spareSliceId, 0.5f}});
    QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 10);
    QTest::qWait(50);
    QCOMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 10);
    QVERIFY(peakOfL16(h.mediaTransport->rtpPackets, mainSsrc) > 0.05f);
    QVERIFY(headphonesContextsIn(controls).isEmpty());
    QCOMPARE(audioContextsIn(controls).size(), 1);
    QVERIFY(!h.controller.headphonesMixSending());
    h.finish();
}

// R-R3-45: with the headphones mix declared, the main stream carries the
// speakers' mix alone. Routing slice B to the headphones starts the second
// mix on its own stream id; it carries B and the main stream keeps A.
// Routing A there too changes only what the mix holds (no new context);
// routing both back stops the stream. The main stream is never restarted.
void TstDaemonMediaController::headphonesMixRunsWhileAReceiverIsOnTheHeadphones()
{
    Harness h;
    const auto routes = qScopeGuard([&h] { resetOutputRoutes(h); });
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(headphonesStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    const quint32 headphonesSsrc =
        MediaPeer::headphonesAudioSsrcForConnection(QLatin1String(kConnectionId));
    QCOMPARE(h.mediaTransport->startOptions.headphonesAudioSsrc, headphonesSsrc);
    QVERIFY(h.mediaTransport->startOptions.receiverAudioSsrcs.isEmpty());
    const quint32 mainSsrc = h.mediaTransport->startOptions.localAudioSsrc;
    QVERIFY(headphonesSsrc != 0 && headphonesSsrc != mainSsrc);
    QVERIFY(!MediaPeer::receiverAudioSsrcsForConnection(QLatin1String(kConnectionId))
                 .contains(headphonesSsrc));
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(audioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(audioContextsIn(controls).size(), 1);

    // Asked for with every receiver on the speakers: off, and why.
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 1);
    std::optional<RemoteAudioContextMessage> context =
        decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value());
    QVERIFY(!context->enabled);
    QCOMPARE(context->offReason, std::optional{RemoteAudioOffReason::NoHeadphonesReceiver});
    QCOMPARE(context->ssrc, headphonesSsrc);
    QCOMPARE(engine->sliceAudioTapCount(), 0);

    // B to the headphones: the mix starts on its own id, lossless.
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 2);
    context = decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value());
    QVERIFY(context->enabled);
    QCOMPARE(context->ssrc, headphonesSsrc);
    QCOMPARE(context->firstSequence, quint16{1});
    QCOMPARE(context->firstTimestamp, quint32{0});
    QCOMPARE(context->profile, std::optional{RemoteAudioProfile::Lossless});
    QVERIFY(context->losslessEncoder.has_value());
    QVERIFY(h.controller.headphonesMixSending());
    QCOMPARE(h.controller.headphonesMixProfile(), std::optional{RemoteAudioProfile::Lossless});

    // A alone plays: it is on the main stream, the headphones mix is silent.
    feedSlicesBlock(h, {{h.sliceId, 0.5f}, {h.spareSliceId, 0.0f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 10);
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc).size(), 10);
    QVERIFY(peakOfL16(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc), mainSsrc)
            > 0.05f);
    QCOMPARE(peakOfL16(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc),
                       headphonesSsrc), 0.0f);
    // B alone plays: it is on the headphones mix, the main stream is silent.
    h.mediaTransport->rtpPackets.clear();
    feedSlicesBlock(h, {{h.sliceId, 0.0f}, {h.spareSliceId, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 10);
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc).size(), 10);
    QCOMPARE(peakOfL16(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc), mainSsrc),
             0.0f);
    QVERIFY(peakOfL16(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc),
                      headphonesSsrc) > 0.05f);

    // A joins B on the headphones and leaves again: the mix changes, the
    // stream does not.
    h.radio.sliceById(h.sliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    h.radio.sliceById(h.sliceId)->setOutputRoute(SliceModel::OutputRoute::Speakers);
    QTest::qWait(50);
    QCOMPARE(headphonesContextsIn(controls).size(), 2);

    // A stale request is ignored.
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, false, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTest::qWait(50);
    QCOMPARE(headphonesContextsIn(controls).size(), 2);
    QVERIFY(h.controller.headphonesMixSending());

    // B back to the speakers: the headphones stream stops, and why.
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Speakers);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 3);
    context = decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value());
    QVERIFY(!context->enabled);
    QCOMPARE(context->offReason, std::optional{RemoteAudioOffReason::NoHeadphonesReceiver});
    QCOMPARE(context->firstSequence, quint16{21});
    QCOMPARE(context->firstTimestamp, quint32{3840});
    QVERIFY(!h.controller.headphonesMixSending());
    h.mediaTransport->rtpPackets.clear();
    feedSlicesBlock(h, {{h.sliceId, 0.0f}, {h.spareSliceId, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc).size(), 10);
    QTest::qWait(50);
    QVERIFY(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc).isEmpty());
    // B is back in the speakers' mix.
    QVERIFY(peakOfL16(packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc), mainSsrc)
            > 0.05f);

    // Back on the headphones, the stream id's timeline continues.
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 4);
    context = decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value() && context->enabled);
    QCOMPARE(context->firstSequence, quint16{21});
    QCOMPARE(context->firstTimestamp, quint32{3840});
    // The app turns it off with B still there: client-disabled.
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(2, false, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 5);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("client-disabled"));

    // One main context for the one main control; none from the headphones.
    QCOMPARE(audioContextsIn(controls).size(), 1);
    const QList<QByteArray> mainPackets =
        packetsWithSsrc(h.mediaTransport->rtpPackets, mainSsrc);
    for (int index = 1; index < mainPackets.size(); ++index) {
        QCOMPARE(rtpSequence(mainPackets.at(index)),
                 static_cast<quint16>(rtpSequence(mainPackets.at(index - 1)) + 1));
    }
    h.finish();
    QTRY_VERIFY(!h.controller.headphonesMixSending());
}

// R-R3-45: the headphones mix follows its request's profile (the session's
// one choice), so the app's one fallback to Opus moves it too; it pauses
// with the radio and resumes with it, and ends with the session.
void TstDaemonMediaController::headphonesMixFollowsTheProfileAndTheRadio()
{
    OpusAudioEncoder encoder;
    if (!encoder.isReady()) {
        QSKIP("Opus encoder is unavailable in this build");
    }
    Harness h;
    // A Core set to 24000, so the speaker-side rate differs from the receiver
    // streams' 48 kbit/s (R-R3-21 made 48000 the default).
    h.controller.setAudioTargetBitrate(24'000);
    const auto routes = qScopeGuard([&h] { resetOutputRoutes(h); });
    AudioEngine* const engine = h.radio.audioEngine();
    engine->masterMixForTest().setRampFrames(1);
    engine->masterMixForTest().setSlewUpFrames(0);
    engine->setSliceStreaming(h.sliceId, true);
    engine->setSliceStreaming(h.spareSliceId, true);
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(headphonesStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    const quint32 headphonesSsrc = h.mediaTransport->startOptions.headphonesAudioSsrc;
    h.mediaTransport->losslessNegotiated = true;
    h.mediaTransport->becomeReady();
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 1);
    QCOMPARE(h.controller.headphonesMixProfile(), std::optional{RemoteAudioProfile::Lossless});

    // The app falls back to Opus: the mix follows, at the Core's bitrate.
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(2, true, QStringLiteral("opus")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 2);
    std::optional<RemoteAudioContextMessage> context =
        decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value() && context->enabled);
    QCOMPARE(context->profile, std::optional{RemoteAudioProfile::Opus});
    QVERIFY(context->encoder.has_value());
    QCOMPARE(context->encoder->targetBitrate, h.controller.audioTargetBitrate());
    // A speaker-side mix: the Core's audio_bitrate (set to 24 kbit/s above),
    // not the receiver streams' 48 kbit/s.
    QCOMPARE(context->encoder->targetBitrate, 24'000);
    feedSlicesBlock(h, {{h.sliceId, 0.0f}, {h.spareSliceId, 0.5f}});
    QTRY_COMPARE(packetsWithSsrc(h.mediaTransport->rtpPackets, headphonesSsrc).size(), 1);

    // Lossless refused by the Core's own setting: Opus, and why.
    h.controller.setAudioLosslessAllowed(false);
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(3, true, QStringLiteral("lossless")),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 3);
    context = decodeHeadphonesAudioContext(headphonesContextsIn(controls).constLast());
    QVERIFY(context.has_value() && context->enabled);
    QCOMPARE(context->profile, std::optional{RemoteAudioProfile::Opus});
    QCOMPARE(context->profileRefusal, std::optional{RemoteAudioProfileRefusal::NotAllowed});
    h.controller.setAudioLosslessAllowed(true);

    // The radio goes offline: the mix stops and says why, then resumes.
    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 4);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("radio-offline"));
    QVERIFY(!h.controller.headphonesMixSending());
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 5);
    QVERIFY(headphonesContextsIn(controls).constLast().value(QStringLiteral("enabled")).toBool());
    QVERIFY(h.controller.headphonesMixSending());

    // The session ends: the mix and its tap go with it.
    h.finish();
    QTRY_VERIFY(!h.controller.headphonesMixSending());
    QTest::qWait(20);
    QCOMPARE(headphonesContextsIn(controls).size(), 5);
}

// R-R3-45 fix wave: a radio drop sends a disabled headphones context only
// when the mix was sending. With no receiver on the headphones the app was
// told no-headphones-receiver, and that stays the reason: nothing is sent,
// and radio-offline never replaces it. A route made while the radio is
// away is answered in reconcileHeadphonesAudio's order (radio-offline).
void TstDaemonMediaController::radioDropKeepsTheHeadphonesReasonWhenNothingIsRouted()
{
    Harness h;
    const auto routes = qScopeGuard([&h] { resetOutputRoutes(h); });
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(headphonesStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 1);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("no-headphones-receiver"));

    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    QTest::qWait(50);
    QCOMPARE(headphonesContextsIn(controls).size(), 1);

    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 2);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("radio-offline"));
    QVERIFY(!h.controller.headphonesMixSending());

    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 3);
    QVERIFY(headphonesContextsIn(controls).constLast().value(QStringLiteral("enabled")).toBool());
    h.finish();
}

// R-R3-45 fix wave (re-review): an app last told media-not-ready learns
// radio-offline when the radio drops, since that is now the reason; a
// second drop notice with the same reason is not sent.
void TstDaemonMediaController::radioDropTellsAnAppWaitingOnMediaThatTheRadioIsGone()
{
    Harness h;
    const auto routes = qScopeGuard([&h] { resetOutputRoutes(h); });
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    QVERIFY(h.client.sendMediaControl(headphonesStart(), h.client.sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.radio.sliceById(h.spareSliceId)->setOutputRoute(SliceModel::OutputRoute::Headphones);
    QVERIFY(h.client.sendMediaControl(headphonesAudioControl(1, true),
                                      h.client.sessionEpoch()));
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 1);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("media-not-ready"));

    h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 2);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("radio-offline"));
    h.radio.setConnectionStateForTest(ConnectionState::Connected);
    QTRY_COMPARE(headphonesContextsIn(controls).size(), 3);
    QCOMPARE(headphonesContextsIn(controls).constLast().value(QStringLiteral("reason")).toString(),
             QStringLiteral("media-not-ready"));
    h.finish();
}

// A full minute at the accepted 192 kHz hardware rate uses one ordered frame
// every five milliseconds. The fake transport verifies the same bytes the
// real dedicated channel receives, without retaining 90 MB of test frames.
void TstDaemonMediaController::rawIqRunsForSixtySecondsInSequenceAndRetires()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QCOMPARE(h.client.capabilities().remoteIqVersion, 1);
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    QVERIFY(h.mediaTransport->startOptions.iqChannel);
    h.useManualDisplayTicks();

    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("enabled"), true}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    quint32 expectedSequence = 0;
    quint32 generation = 0;
    bool framesValid = true;
    h.mediaTransport->onIqSend = [&](const QByteArray& bytes) {
        const auto frame = RemoteIqCodec::decode(bytes);
        if (!frame || frame->sliceId != quint32(h.sliceId)
            || frame->sequence != expectedSequence
            || frame->samples.size() != 2048
            || (generation && frame->generation != generation)) {
            framesValid = false;
            return;
        }
        generation = frame->generation;
        ++expectedSequence;
    };
    const QVector<float> block(1920, 0.25f);
    for (int tick = 0; tick < 12'000 && framesValid; ++tick) {
        h.radio.rawIqDataForStream(h.streamIndex, block);
        h.sendDisplayTick();
        h.nowNs += 5'000'000;
    }
    QVERIFY(framesValid);
    QCOMPARE(expectedSequence, quint32(11'250));
    QVERIFY(generation != 0);

    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 2},
        {QStringLiteral("enabled"), false}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("revision")).toInt() == 2
                && !context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    h.radio.rawIqDataForStream(h.streamIndex, block);
    h.sendDisplayTick();
    QCOMPARE(expectedSequence, quint32(11'250));
    h.finish();
}

void TstDaemonMediaController::stalledRawIqFailsWithoutBlockingControl()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    h.useManualDisplayTicks();
    h.mediaTransport->stallIq = true;
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("enabled"), true}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    const QVector<float> block(2048, 0.25f);
    h.radio.rawIqDataForStream(h.streamIndex, block);
    h.sendDisplayTick();
    // Control remains responsive while the IQ SCTP channel is wedged.
    QElapsedTimer controlClock;
    controlClock.start();
    QVERIFY(h.client.sendMediaControl(subscription(1, 1, h.sliceId,
        h.radio.streamCentreHz(h.streamIndex)), h.client.sessionEpoch()));
    QTRY_VERIFY(!allocationFor(controls, 1, 1).isEmpty());
    QVERIFY(controlClock.elapsed() < 1000);
    h.nowNs = 250'000'000;
    h.sendDisplayTick();
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("revision")).toInt() == 1
                && !context.value(QStringLiteral("enabled")).toBool()
                && context.value(QStringLiteral("reason")).toString().contains(
                    QLatin1String("stalled"))) { return true; }
        }
        return false;
    })());
    const int stoppedContexts = [&] {
        int count = 0;
        for (const auto& call : controls) {
            if (call.at(0).toJsonObject().value(QStringLiteral("op"))
                == QLatin1String("iq-stream-context")) { ++count; }
        }
        return count;
    }();
    h.mediaTransport->stallIq = false;
    QVERIFY(h.server.setDisplayBudgetLimits({9'000'000, 9'000'000, 2}));
    QTest::qWait(30);
    QCOMPARE(([&] {
        int count = 0;
        for (const auto& call : controls) {
            if (call.at(0).toJsonObject().value(QStringLiteral("op"))
                == QLatin1String("iq-stream-context")) { ++count; }
        }
        return count;
    })(), stoppedContexts);
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 2},
        {QStringLiteral("enabled"), true}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("revision")).toInt() == 2
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    h.finish();
}

void TstDaemonMediaController::rawIqBusyRetryDebitsOnlyOnce()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    h.useManualDisplayTicks();
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("enabled"), true}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    QList<quint32> sequences;
    h.mediaTransport->onIqSend = [&](const QByteArray& bytes) {
        const auto frame = RemoteIqCodec::decode(bytes);
        if (frame) { sequences.append(frame->sequence); }
    };
    const QVector<float> block(2048, 0.25f);
    for (int i = 0; i < 2; ++i) {
        h.radio.rawIqDataForStream(h.streamIndex, block);
        h.sendDisplayTick();
    }
    QCOMPARE(sequences, (QList<quint32>{0, 1}));
    h.radio.rawIqDataForStream(h.streamIndex, block);
    h.mediaTransport->nextIqSubmitResult = IMediaTransport::DisplaySendResult::Busy;
    h.sendDisplayTick();
    QCOMPARE(sequences, (QList<quint32>{0, 1}));
    // The three-frame IQ burst is now exhausted. Retrying the same pending
    // frame at the same clock instant succeeds only if it is not debited twice.
    h.sendDisplayTick();
    QCOMPARE(sequences, (QList<quint32>{0, 1, 2}));
    h.finish();
}

void TstDaemonMediaController::rawIqSubmitBusyStopsAtDeadline()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    h.useManualDisplayTicks();
    QVERIFY(h.client.sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), h.sliceId},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("enabled"), true}}, h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    h.mediaTransport->submitIqBusy = true;
    QVERIFY(!h.mediaTransport->iqBusy());
    h.radio.rawIqDataForStream(h.streamIndex, QVector<float>(2048, 0.25f));
    h.sendDisplayTick();
    h.nowNs = 249'000'000;
    h.sendDisplayTick();
    QVERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && !context.value(QStringLiteral("enabled")).toBool()) { return false; }
        }
        return true;
    })());
    h.nowNs = 250'000'000;
    h.sendDisplayTick();
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && !context.value(QStringLiteral("enabled")).toBool()
                && context.value(QStringLiteral("reason")).toString().contains(
                    QLatin1String("stalled"))) { return true; }
        }
        return false;
    })());
    h.finish();
}

void TstDaemonMediaController::synchronousIqClosureRetiresPeerBeforeNextSend()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    h.useManualDisplayTicks();
    const auto request = [&](int revision) {
        return QJsonObject{{QStringLiteral("op"), QStringLiteral("iq-stream")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("sliceId"), h.sliceId},
            {QStringLiteral("revision"), revision},
            {QStringLiteral("enabled"), true}};
    };
    QVERIFY(h.client.sendMediaControl(request(1), h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    QPointer<QObject> retiredPeer = h.mediaTransport->parent();
    bool closedInsideSubmit = false;
    h.mediaTransport->onIqSend = [transport = h.mediaTransport, &closedInsideSubmit](
                                     const QByteArray&) {
        emit transport->closed();
        closedInsideSubmit = true;
    };
    h.radio.rawIqDataForStream(h.streamIndex, QVector<float>(2048, 0.25f));
    h.sendDisplayTick();
    QVERIFY(closedInsideSubmit);
    QTRY_VERIFY(retiredPeer.isNull() && h.mediaTransport.isNull());

    // The control session survives; a new peer starts a fresh I/Q sequence.
    h.startReadyPeer(true);
    QVERIFY(h.client.sendMediaControl(request(1), h.client.sessionEpoch()));
    QTRY_VERIFY(([&] {
        for (const auto& call : controls) {
            const auto context = call.at(0).toJsonObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()
                && context.value(QStringLiteral("generation")).toInt() > 1) { return true; }
        }
        return false;
    })());
    quint32 firstSequence = std::numeric_limits<quint32>::max();
    h.mediaTransport->onIqSend = [&](const QByteArray& bytes) {
        const auto frame = RemoteIqCodec::decode(bytes);
        if (frame) { firstSequence = frame->sequence; }
    };
    h.radio.rawIqDataForStream(h.streamIndex, QVector<float>(2048, 0.25f));
    h.sendDisplayTick();
    QCOMPARE(firstSequence, quint32(0));
    h.finish();
}

void TstDaemonMediaController::rawIqControlRejectsMalformedAndUnownedRequests()
{
    Harness h(DisplayBudgetLimits{10'000'000, 10'000'000, 1});
    h.establishSession();
    QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
    h.startReadyPeer(true);
    const auto contexts = [&] {
        QList<QJsonObject> out;
        for (const auto& call : controls) {
            const QJsonObject message = call.at(0).toJsonObject();
            if (message.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")) {
                out.append(message);
            }
        }
        return out;
    };
    const auto request = [&](int slice, int revision) {
        return QJsonObject{{QStringLiteral("op"), QStringLiteral("iq-stream")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("sliceId"), slice},
            {QStringLiteral("revision"), revision},
            {QStringLiteral("enabled"), true}};
    };
    QJsonObject malformed = request(h.sliceId, 1);
    malformed.insert(QStringLiteral("extra"), 1);
    QVERIFY(h.client.sendMediaControl(malformed, h.client.sessionEpoch()));
    malformed = request(h.sliceId, 0);
    QVERIFY(h.client.sendMediaControl(malformed, h.client.sessionEpoch()));
    malformed = request(h.sliceId, 1);
    malformed.insert(QStringLiteral("connectionId"),
                     QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
    QVERIFY(h.client.sendMediaControl(malformed, h.client.sessionEpoch()));
    QTest::qWait(20);
    QCOMPARE(contexts().size(), 0);

    QVERIFY(h.client.sendMediaControl(request(h.sliceId, 1), h.client.sessionEpoch()));
    QTRY_COMPARE(contexts().size(), 1);
    const QJsonObject accepted = contexts().at(0);
    QCOMPARE(accepted.size(), 8);
    QVERIFY(accepted.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(accepted.value(QStringLiteral("sampleRateHz")).toInt(), 192000);
    QVERIFY(h.client.sendMediaControl(request(h.sliceId, 1), h.client.sessionEpoch()));
    QTest::qWait(20);
    QCOMPARE(contexts().size(), 1);

    QVERIFY(h.client.sendMediaControl(request(999, 1), h.client.sessionEpoch()));
    QTRY_COMPARE(contexts().size(), 2);
    const QJsonObject refused = contexts().at(1);
    QCOMPARE(refused.size(), 8);
    QVERIFY(!refused.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(refused.value(QStringLiteral("sampleRateHz")).toInt(), 0);
    QVERIFY(!refused.value(QStringLiteral("reason")).toString().isEmpty());
    h.finish();
}
